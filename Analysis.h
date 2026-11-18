#pragma once
// Background game review: a second Stockfish evaluates every position of the game,
// then each move gets a grade based on how much it lowered the player's winning chances.
#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "Engine.h"

const int ANALYSIS_MS = 250; // Stockfish time per position when reviewing (first pass)
const int DEEP_MS = 1500;    // time for double-checking positions around a possible mistake/blunder
const double CHECK_DROP = 10; // moves losing at least this much winning chance get double-checked

// evaluation of one position, always from WHITE's point of view
struct Eval
{
    bool done = false;
    bool mate = false;
    int winner = 0;   // when mate: +1 white mates, -1 black mates
    int mateIn = 0;   // when mate: moves until mate (0 = already mate on the board)
    int cp = 0;       // when not mate: centipawns, + is good for white
    std::string best; // Stockfish's best move here (UCI), "" if game over
    bool deep = false; // analysed with the longer think time
};

// chance (0..100) that `color` wins from this eval. Same curve lichess uses.
// Evals are capped at +-10 pawns and a forced mate counts as +-10, so going from
// "mate in 5" to "+15" (still totally winning) is not a big loss.
inline double winPercent(const Eval &e, int color)
{
    int cp = e.mate ? 1000 * e.winner : std::clamp(e.cp, -1000, 1000);
    double white = 50.0 + 50.0 * (2.0 / (1.0 + std::exp(-0.00368208 * cp)) - 1.0);
    return color > 0 ? white : 100.0 - white;
}

// "+0.45", "-2.10", "M3", "-M2", "1-0"
inline std::string evalText(const Eval &e)
{
    if (!e.done)
        return "...";
    if (e.mate)
    {
        if (e.mateIn == 0)
            return e.winner > 0 ? "1-0" : "0-1";
        return std::string(e.winner > 0 ? "M" : "-M") + std::to_string(e.mateIn);
    }
    char buf[16];
    snprintf(buf, sizeof buf, "%+.2f", e.cp / 100.0);
    return buf;
}

enum class Grade
{
    None,
    Book,
    Brilliand,
    Great,
    Okay,
    Miss,
    Inaccuracy,
    Mistake,
    Blunder
};

struct MoveReview
{
    Grade grade = Grade::None;
    double accuracy = 0; // 0..100, how much Stockfish liked the move
    double drop = 0;     // how many % of winning chance the move threw away
};

// grade of the move that goes from position `before` to `after`. mover = WHITE/BLACK
// prevBefore = position before the opponent's last move (nullptr for the first move)
// isBook = the position after the move is known opening theory
inline MoveReview reviewMove(const Eval *prevBefore, const Eval &before, const Eval &after, int mover,const std::string &playedUci, bool isBook)
{
    MoveReview r;
    if (isBook)
    {
        r.grade = Grade::Book;
        r.accuracy = 100;
        return r;
    }
    if (!before.done || !after.done)
        return r;

    double wpBefore = winPercent(before, mover), wpAfter = winPercent(after, mover);
    r.drop = std::max(0.0, wpBefore - wpAfter);
    r.accuracy = std::clamp(103.1668 * std::exp(-0.04354 * r.drop) - 3.1669, 0.0, 100.0);

    if (!before.best.empty() && playedUci.substr(0, 4) == before.best.substr(0, 4))
    {
        r.grade = Grade::Brilliand;
        r.accuracy = 100;
        return r;
    }
    if (r.drop < 2)
        r.grade = Grade::Great;
    else if (r.drop < 5)
        r.grade = Grade::Okay;
    else if (r.drop < 10)
        r.grade = Grade::Inaccuracy;
    else if (r.drop < 20)
        r.grade = Grade::Mistake;
    else
        r.grade = Grade::Blunder;

    // Miss: the opponent's last move handed you a chance (>= 10% winning chance),
    // you didn't take it, but you are no worse off than before their mistake.
    if (r.drop >= 10 && prevBefore && prevBefore->done)
    {
        double wpPrev = winPercent(*prevBefore, mover);
        double gift = wpBefore - wpPrev;
        if (gift >= 10 && wpAfter >= wpPrev - 5)
            r.grade = Grade::Miss;
    }
    return r;
}

class Analyzer
{
public:
    bool start()
    {
        if (!engine.start("stockfish"))
            return false;
        unsigned threads = std::max(1u, std::thread::hardware_concurrency() / 2);
        engine.setOption("Threads", std::to_string(threads));
        engine.setOption("Hash", "128");
        ok = true;
        worker = std::thread([this]() { loop(); });
        return true;
    }

    ~Analyzer()
    {
        {
            std::lock_guard<std::mutex> lock(mx);
            quit = true;
        }
        cv.notify_all();
        if (worker.joinable())
            worker.join();
    }

    bool running() const { return ok; }

    // tell the analyzer the current list of moves. Positions it already analysed stay cached.
    void setMoves(const std::vector<std::string> &newMoves)
    {
        std::lock_guard<std::mutex> lock(mx);
        size_t same = 0;
        while (same < moves.size() && same < newMoves.size() && moves[same] == newMoves[same])
            same++;
        moves = newMoves;
        evals.resize(moves.size() + 1);
        for (size_t i = same + 1; i < evals.size(); i++)
            evals[i] = Eval();
        cv.notify_all();
    }

    // analyse this position (and the one before it) first
    void setFocus(int index)
    {
        std::lock_guard<std::mutex> lock(mx);
        focus = index;
        cv.notify_all();
    }

    // how many moves are waiting to be double-checked
    int pendingChecks()
    {
        std::lock_guard<std::mutex> lock(mx);
        int n = 0, which;
        for (int i = 0; i + 1 < int(evals.size()); i++)
            n += needsDeep(i, which);
        return n;
    }

    std::vector<Eval> snapshot()
    {
        std::lock_guard<std::mutex> lock(mx);
        return evals;
    }

private:
    Engine engine;
    std::thread worker;
    std::mutex mx;
    std::condition_variable cv;
    std::vector<std::string> moves;
    std::vector<Eval> evals = std::vector<Eval>(1);
    int focus = 0;
    bool quit = false;
    bool ok = false;

    // does the move from position i to i+1 look like it lost a lot? then it gets double-checked
    bool needsDeep(int i, int &which) const
    {
        if (i < 0 || i + 1 >= int(evals.size()) || !evals[i].done || !evals[i + 1].done)
            return false;
        if (evals[i].deep && evals[i + 1].deep)
            return false;
        int mover = (i % 2 == 0) ? 1 : -1;
        if (winPercent(evals[i], mover) - winPercent(evals[i + 1], mover) < CHECK_DROP)
            return false;
        if (!evals[i].best.empty() && i < int(moves.size()) && moves[i].substr(0, 4) == evals[i].best.substr(0, 4))
            return false; // it was Stockfish's own first choice
        which = evals[i].deep ? i + 1 : i;
        return true;
    }

    // which position to analyse next, and whether with the long think time. -1 = nothing to do
    int pickNext(bool &deep) const // must hold the lock
    {
        deep = false;
        // 1. quick pass: the position on screen first, then everything in order
        for (int i : {focus - 1, focus})
            if (i >= 0 && i < int(evals.size()) && !evals[i].done)
                return i;
        for (size_t i = 0; i < evals.size(); i++)
            if (!evals[i].done)
                return int(i);
        // 2. double-check the big drops: the move on screen first
        deep = true;
        int which;
        if (needsDeep(focus - 1, which))
            return which;
        for (int i = 0; i + 1 < int(evals.size()); i++)
            if (needsDeep(i, which))
                return which;
        return -1;
    }

    void loop()
    {
        while (true)
        {
            int index;
            bool deep = false;
            std::vector<std::string> prefix;
            {
                std::unique_lock<std::mutex> lock(mx);
                bool d;
                cv.wait(lock, [this, &d]() { return quit || pickNext(d) != -1; });
                if (quit)
                    return;
                index = pickNext(deep);
                prefix.assign(moves.begin(), moves.begin() + index);
            }

            Engine::Result res = engine.analyse(prefix, deep ? DEEP_MS : ANALYSIS_MS);
            if (!res.ok)
                return; // Stockfish stopped responding

            std::lock_guard<std::mutex> lock(mx);
            // the game may have changed while Stockfish was thinking - only keep it if still valid
            if (index >= int(evals.size()) ||
                !std::equal(prefix.begin(), prefix.end(), moves.begin()))
                continue;

            int sideToMove = (index % 2 == 0) ? 1 : -1;
            Eval e;
            e.done = true;
            e.deep = deep;
            e.best = res.best;
            if (res.mate)
            {
                e.mate = true;
                e.mateIn = std::abs(res.score);
                e.winner = res.score > 0 ? sideToMove : -sideToMove;
            }
            else
                e.cp = res.score * sideToMove;
            evals[index] = e;
        }
    }
};
