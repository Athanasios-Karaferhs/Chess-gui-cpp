#pragma once
// Talks to Stockfish (or any UCI engine) through pipes. Linux/macOS only (fork + exec).
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

class Engine
{
public:
    ~Engine() { stop(); }

    // starts the engine and does the UCI handshake. returns false if it can't be found/started
    bool start(const std::string &command = "stockfish")
    {
        int toChild[2], fromChild[2];
        if (pipe(toChild) != 0)
            return false;
        if (pipe(fromChild) != 0)
        {
            close(toChild[0]);
            close(toChild[1]);
            return false;
        }

        signal(SIGPIPE, SIG_IGN); // don't crash if the engine dies

        pid = fork();
        if (pid < 0)
            return false;
        if (pid == 0)
        {
            // child: stdin <- toChild, stdout -> fromChild
            dup2(toChild[0], STDIN_FILENO);
            dup2(fromChild[1], STDOUT_FILENO);
            close(toChild[0]);
            close(toChild[1]);
            close(fromChild[0]);
            close(fromChild[1]);
            execlp(command.c_str(), command.c_str(), (char *)nullptr);
            _exit(127); // exec failed
        }

        close(toChild[0]);
        close(fromChild[1]);
        toEngine = fdopen(toChild[1], "w");
        fromEngine = fdopen(fromChild[0], "r");
        if (!toEngine || !fromEngine)
        {
            stop();
            return false;
        }

        send("uci");
        if (!waitFor("uciok"))
        {
            stop();
            return false;
        }
        return isReady();
    }

    bool running() const { return pid > 0; }

    void newGame()
    {
        send("ucinewgame");
        isReady();
    }

    // 0 (weakest) .. 20 (full strength)
    void setSkillLevel(int level)
    {
        send("setoption name Skill Level value " + std::to_string(level));
        isReady();
    }

    void setOption(const std::string &name, const std::string &value)
    {
        send("setoption name " + name + " value " + value);
        isReady();
    }

    // what Stockfish thinks of a position. score is from the point of view of the SIDE TO MOVE.
    struct Result
    {
        bool ok = false;
        bool mate = false; // true: `score` is "mate in N" (N <= 0 means the side to move is getting mated)
        int score = 0;     // centipawns (100 = one pawn) or mate distance
        std::string best;  // best move in UCI form, "" if there is none (game over)
    };

    // moves = all moves played since the start, in UCI form ("e2e4"...)
    Result analyse(const std::vector<std::string> &moves, int thinkMs)
    {
        Result res;
        std::string pos = "position startpos";
        if (!moves.empty())
        {
            pos += " moves";
            for (const std::string &m : moves)
                pos += " " + m;
        }
        send(pos);
        send("go movetime " + std::to_string(thinkMs));

        std::string line;
        while (readLine(line))
        {
            std::vector<std::string> tok = split(line);
            if (tok.empty())
                continue;
            if (tok[0] == "info")
            {
                bool bound = false;
                for (auto &t : tok)
                    bound |= (t == "lowerbound" || t == "upperbound");
                for (size_t i = 0; i + 2 < tok.size() && !bound; i++)
                    if (tok[i] == "score")
                    {
                        res.mate = (tok[i + 1] == "mate");
                        res.score = std::atoi(tok[i + 2].c_str());
                    }
            }
            else if (tok[0] == "bestmove")
            {
                res.ok = true;
                if (tok.size() > 1 && tok[1] != "(none)")
                    res.best = tok[1];
                return res;
            }
        }
        return res;
    }

    // returns e.g. "e7e5" or "" on error
    std::string bestMove(const std::vector<std::string> &moves, int thinkMs)
    {
        return analyse(moves, thinkMs).best;
    }

    void stop()
    {
        if (toEngine)
        {
            send("quit");
            fclose(toEngine);
            toEngine = nullptr;
        }
        if (fromEngine)
        {
            fclose(fromEngine);
            fromEngine = nullptr;
        }
        if (pid > 0)
        {
            waitpid(pid, nullptr, 0);
            pid = -1;
        }
    }

private:
    pid_t pid = -1;
    FILE *toEngine = nullptr;
    FILE *fromEngine = nullptr;

    void send(const std::string &cmd)
    {
        if (!toEngine)
            return;
        fprintf(toEngine, "%s\n", cmd.c_str());
        fflush(toEngine);
    }

    bool readLine(std::string &out)
    {
        if (!fromEngine)
            return false;
        char *buf = nullptr;
        size_t cap = 0;
        ssize_t n = getline(&buf, &cap, fromEngine);
        if (n < 0)
        {
            free(buf);
            return false;
        }
        out.assign(buf, n);
        free(buf);
        while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
            out.pop_back();
        return true;
    }

    static std::vector<std::string> split(const std::string &line)
    {
        std::vector<std::string> out;
        size_t i = 0;
        while (i < line.size())
        {
            while (i < line.size() && line[i] == ' ')
                i++;
            size_t j = i;
            while (j < line.size() && line[j] != ' ')
                j++;
            if (j > i)
                out.push_back(line.substr(i, j - i));
            i = j;
        }
        return out;
    }

    bool waitFor(const std::string &token)
    {
        std::string line;
        while (readLine(line))
            if (line == token)
                return true;
        return false;
    }

    bool isReady()
    {
        send("isready");
        return waitFor("readyok");
    }
};
