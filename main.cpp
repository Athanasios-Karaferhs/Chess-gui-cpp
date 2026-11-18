#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include "Board.h"
#include "Rules.h"
#include "Gamestate.h"
#include "Engine.h"
#include "Analysis.h"
#include "Pgn.h"
#include "Openings.h"
#include "ChessCom.h"

const int ENGINE_THINK_MS = 800;  // how long Stockfish thinks per move when you play against it
const float ANIM_SECONDS = 0.20f; // how long a piece takes to slide

enum class Mode
{
    Analysis,        // you move both sides (also used for reviewing imported games)
    VsEngineAsWhite, // you are white, Stockfish is black
    VsEngineAsBlack  // you are black, Stockfish is white
};

// one position of the game. status: 0 normal, 1 check, 2 checkmate, 3 stalemate
struct Node
{
    GameState st;
    int status = 0;
};

// one move (a "ply") of the game
struct Ply
{
    std::string uci, san;
    int fr, fc, tr, tc;
    bool capture;
};

// where everything goes on screen. Recomputed every frame so resizing / fullscreen just works.
struct Layout
{
    float W, H, sq, boardX, boardY, evalX, evalW, panelX, panelW, panelY, panelH, barH;
};

Layout computeLayout(sf::Vector2u size)
{
    Layout L;
    L.W = float(size.x);
    L.H = float(size.y);
    const float m = 14;
    L.evalW = 28;
    L.barH = 32; // player name above and below the board
    L.panelW = std::clamp(L.W * 0.30f, 320.f, 430.f);
    L.sq = std::floor(std::min((L.H - 2 * m - 2 * L.barH) / 8.f, (L.W - L.panelW - L.evalW - 4 * m) / 8.f));
    L.sq = std::max(L.sq, 20.f);
    float boardPx = 8 * L.sq;
    float leftSpace = L.W - L.panelW - m;
    float groupW = L.evalW + m + boardPx;
    L.evalX = std::floor(std::max(m, (leftSpace - groupW) / 2.f));
    L.boardX = L.evalX + L.evalW + m;
    L.boardY = std::floor((L.H - boardPx) / 2.f);
    L.panelX = L.boardX + boardPx + m;
    L.panelW = L.W - L.panelX - m;
    L.panelY = L.boardY - L.barH;
    L.panelH = boardPx + 2 * L.barH;
    return L;
}

// ---------------------------------------------------------------- animation
struct Slide
{
    int piece, fr, fc, tr, tc;
};
struct Fade
{
    int piece, row, col;
    bool in;
};
struct Animation
{
    bool active = false;
    sf::Clock clock;
    std::vector<Slide> slides;
    std::vector<Fade> fades;
    bool hide[8][8] = {}; // squares of the new board that are drawn by the animation instead

    float progress() const { return std::min(1.f, clock.getElapsedTime().asSeconds() / ANIM_SECONDS); }
};

// Works out how to animate from one board to another: pieces that moved slide,
// captured pieces fade out, pieces that come back (when stepping backwards) fade in.
Animation makeAnimation(const Board &from, const Board &to)
{
    Animation a;
    std::vector<std::pair<int, int>> gone, came;
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
        {
            if (from.at(r, c) != 0 && from.at(r, c) != to.at(r, c))
                gone.push_back({r, c});
            if (to.at(r, c) != 0 && to.at(r, c) != from.at(r, c))
                came.push_back({r, c});
        }
    std::vector<bool> goneUsed(gone.size(), false), cameUsed(came.size(), false);

    // pass 0: same piece. pass 1: promotion (pawn <-> queen of the same colour)
    for (int pass = 0; pass < 2; pass++)
        for (size_t i = 0; i < came.size(); i++)
        {
            if (cameUsed[i])
                continue;
            int want = to.at(came[i].first, came[i].second);
            int best = -1, bestDist = 1000;
            for (size_t j = 0; j < gone.size(); j++)
            {
                if (goneUsed[j])
                    continue;
                int have = from.at(gone[j].first, gone[j].second);
                bool match = (pass == 0) ? have == want
                                         : (have > 0) == (want > 0) &&
                                               ((std::abs(have) == 1 && std::abs(want) == 6) ||
                                                (std::abs(have) == 6 && std::abs(want) == 1));
                int dist = std::abs(gone[j].first - came[i].first) + std::abs(gone[j].second - came[i].second);
                if (match && dist < bestDist)
                {
                    best = int(j);
                    bestDist = dist;
                }
            }
            if (best != -1)
            {
                goneUsed[best] = cameUsed[i] = true;
                auto [fr, fc] = gone[best];
                a.slides.push_back({from.at(fr, fc), fr, fc, came[i].first, came[i].second});
                a.hide[came[i].first][came[i].second] = true;
            }
        }
    for (size_t j = 0; j < gone.size(); j++)
        if (!goneUsed[j])
            a.fades.push_back({from.at(gone[j].first, gone[j].second), gone[j].first, gone[j].second, false});
    for (size_t i = 0; i < came.size(); i++)
        if (!cameUsed[i])
        {
            a.fades.push_back({to.at(came[i].first, came[i].second), came[i].first, came[i].second, true});
            a.hide[came[i].first][came[i].second] = true;
        }
    a.active = !a.slides.empty() || !a.fades.empty();
    a.clock.restart();
    return a;
}

float easeOutCubic(float t) { return 1 - std::pow(1 - t, 3.f); }
float easeOutBack(float t)
{
    const float c1 = 1.70158f, c3 = c1 + 1;
    return 1 + c3 * std::pow(t - 1, 3.f) + c1 * std::pow(t - 1, 2.f);
}

// ---------------------------------------------------------------- sounds
// makes a sound in code, so no sound files are needed. notes = {frequency Hz, seconds}, 0 Hz = silence
sf::SoundBuffer makeTone(const std::vector<std::pair<float, float>> &notes, float volume = 0.35f)
{
    const unsigned rate = 44100;
    std::vector<std::int16_t> samples;
    for (auto [freq, seconds] : notes)
    {
        size_t n = static_cast<size_t>(rate * seconds);
        for (size_t i = 0; i < n; i++)
        {
            float t = float(i) / rate;
            float fadeIn = std::min(1.0f, i / (0.004f * rate));
            float fadeOut = std::min(1.0f, (n - i) / (0.03f * rate));
            float v = freq > 0 ? std::sin(2.0f * 3.14159265f * freq * t) : 0.0f;
            samples.push_back(static_cast<std::int16_t>(volume * 32767 * fadeIn * fadeOut * v));
        }
    }
    sf::SoundBuffer buffer;
    if (!buffer.loadFromSamples(samples.data(), samples.size(), 1, rate, {sf::SoundChannel::Mono}))
        std::cerr << "Failed to create sound\n";
    return buffer;
}

// ---------------------------------------------------------------- move grades
struct GradeStyle
{
    std::string name, symbol;
    sf::Color color;
};

GradeStyle gradeStyle(Grade g)
{
    switch (g)
    {
    case Grade::Book: return {"Book move", "▤", sf::Color(160, 125, 90)};
    case Grade::Best: return {"Best move", "★", sf::Color(86, 160, 60)};
    case Grade::Excellent: return {"Excellent", "!", sf::Color(96, 180, 110)};
    case Grade::Good: return {"Good", "✓", sf::Color(110, 145, 110)};
    case Grade::Miss: return {"Miss", "✕", sf::Color(215, 85, 110)};
    case Grade::Inaccuracy: return {"Inaccuracy", "?!", sf::Color(225, 175, 30)};
    case Grade::Mistake: return {"Mistake", "?", sf::Color(235, 120, 40)};
    case Grade::Blunder: return {"Blunder", "??", sf::Color(205, 45, 45)};
    default: return {"", "", sf::Color(120, 120, 120)};
    }
}

std::string readFile(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// your chess.com username is remembered in ~/.config/chess-gui-cpp/settings.txt
std::string settingsPath()
{
    const char *home = std::getenv("HOME");
    return std::string(home ? home : ".") + "/.config/chess-gui-cpp/settings.txt";
}

std::string loadUsername()
{
    std::ifstream in(settingsPath());
    std::string line;
    while (std::getline(in, line))
        if (line.rfind("username=", 0) == 0)
            return line.substr(9);
    return "";
}

void saveUsername(const std::string &name)
{
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(settingsPath()).parent_path(), ec);
    std::ofstream out(settingsPath());
    out << "username=" << name << "\n";
}

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

int main(int argc, char **argv)
{
    // ------------------------------------------------------------ window
    sf::RenderWindow window;
    bool fullscreen = false;
    auto createWindow = [&]()
    {
        sf::ContextSettings settings;
        settings.antiAliasingLevel = 4;
        if (fullscreen)
            window.create(sf::VideoMode::getDesktopMode(), "Chess :)", sf::State::Fullscreen, settings);
        else
        {
            sf::Vector2u desk = sf::VideoMode::getDesktopMode().size;
            unsigned w = std::min(1180u, unsigned(desk.x * 0.9)), h = std::min(760u, unsigned(desk.y * 0.85));
            window.create(sf::VideoMode({w, h}), "Chess :)", sf::Style::Default, sf::State::Windowed, settings);
            window.setMinimumSize(sf::Vector2u{760, 540});
        }
        window.setFramerateLimit(60);
        sf::Vector2f size(window.getSize());
        window.setView(sf::View(sf::FloatRect({0.f, 0.f}, size)));
    };
    createWindow();

    // load το font για τα γραμματα
    sf::Font font;
    if (!font.openFromFile("AdwaitaMono-Regular.ttf"))
    {
        std::cerr << "Failed to load font\n";
        return -1;
    }

    // textures: index = piece value + 6  (so -6..6 -> 0..12)
    sf::Texture textures[13];
    const char *files[13] = {
        "pieces/black_que.png", "pieces/black_king.png", "pieces/black_ro.png", "pieces/black_bis.png",
        "pieces/black_horse.png", "pieces/black_po.png", nullptr, "pieces/white_po.png",
        "pieces/white_horse.png", "pieces/white_bis.png", "pieces/white_rook.png", "pieces/white_king.png",
        "pieces/white_queen.png"};
    for (int i = 0; i < 13; i++)
    {
        if (files[i] && !textures[i].loadFromFile(files[i]))
        {
            std::cerr << "Failed to load " << files[i] << "\n";
            return -1;
        }
        textures[i].setSmooth(true);
    }

    // sounds
    sf::SoundBuffer moveBuffer = makeTone({{440, 0.05f}}, 0.25f);
    sf::SoundBuffer captureBuffer = makeTone({{330, 0.04f}, {247, 0.06f}}, 0.3f);
    sf::SoundBuffer checkBuffer = makeTone({{880, 0.08f}, {0, 0.04f}, {880, 0.12f}});
    sf::SoundBuffer mateBuffer = makeTone({{660, 0.18f}, {550, 0.18f}, {440, 0.5f}});
    sf::Sound moveSound(moveBuffer), captureSound(captureBuffer), checkSound(checkBuffer), mateSound(mateBuffer);

    // ------------------------------------------------------------ Stockfish
    Engine player; // plays against you
    bool playerOk = player.start("stockfish");
    Analyzer analyzer; // reviews the game in the background
    bool analyzerOk = analyzer.start();
    if (!playerOk || !analyzerOk)
        std::cerr << "Could not start Stockfish - playing against it and game review are disabled\n";
    int engineLevel = 20; // Stockfish "Skill Level" 0..20
    std::future<std::string> engineFuture;
    int engineRequestGen = -1;
    std::future<std::string> openFuture; // zenity file dialog

    OpeningBook book;
    if (!book.load("openings.tsv"))
        std::cerr << "openings.tsv not found - no 'Book' moves\n";

    // chess.com games window
    bool gamesOpen = false, ignoreText = false;
    std::string userInput = loadUsername();
    ChessComFetch cc;
    std::future<ChessComFetch> ccFuture;
    std::future<std::pair<std::vector<ChessComGame>, std::string>> ccMoreFuture;
    int ccScroll = 0;

    // ------------------------------------------------------------ game
    std::vector<Node> nodes(1);
    nodes[0].st.reset();
    std::vector<Ply> plies;
    int view = 0; // which position is on the board (0 = start, plies.size() = latest)
    int gameGen = 0; // changes whenever the move list is cut/replaced, so old Stockfish answers get ignored
    Mode mode = playerOk ? Mode::VsEngineAsWhite : Mode::Analysis;
    bool flip = false;
    bool showHint = false;
    std::string whiteName, blackName, resultTag, lastPgn;
    int youColor = 0; // which side you played in a chess.com game (0 = unknown)

    int selRow = -1, selCol = -1;
    bool dragging = false, pressOnSelected = false;
    sf::Vector2f mousePos;

    Animation anim;
    sf::Clock badgeClock, bannerClock, frameClock, frameClockTotal;
    float evalBarShown = 0.5f, accuracyMarkerShown = 50.f;
    int listScroll = 0;
    int lastScrolledView = -1;

    std::string flashMsg;
    sf::Clock flashClock;
    auto flash = [&](const std::string &msg)
    {
        flashMsg = msg;
        flashClock.restart();
    };

    struct Button
    {
        sf::FloatRect rect;
        std::function<void()> action;
    };
    std::vector<Button> buttons;        // rebuilt every frame while drawing
    std::vector<Button> overlayButtons; // buttons of the chess.com window

    auto engineColor = [&]() -> int
    {
        if (mode == Mode::VsEngineAsWhite) return BLACK;
        if (mode == Mode::VsEngineAsBlack) return WHITE;
        return 0;
    };
    auto uciList = [&]()
    {
        std::vector<std::string> v;
        for (auto &p : plies)
            v.push_back(p.uci);
        return v;
    };
    auto last = [&]() { return int(plies.size()); };
    auto cur = [&]() -> const GameState & { return nodes[view].st; };

    auto playPlySound = [&](int plyIndex)
    {
        int status = nodes[plyIndex + 1].status;
        if (status == 2) mateSound.play();
        else if (status == 1) checkSound.play();
        else if (plies[plyIndex].capture) captureSound.play();
        else moveSound.play();
    };

    auto clearSelection = [&]()
    {
        selRow = selCol = -1;
        dragging = false;
    };

    auto goTo = [&](int index)
    {
        index = std::clamp(index, 0, last());
        if (index == view)
            return;
        int prev = view;
        anim = makeAnimation(nodes[view].st.board, nodes[index].st.board);
        view = index;
        clearSelection();
        badgeClock.restart();
        bannerClock.restart();
        if (index == prev + 1)
            playPlySound(prev);
        analyzer.setFocus(view);
    };

    auto waitEngine = [&]()
    {
        if (engineFuture.valid())
            engineFuture.wait();
        engineFuture = std::future<std::string>();
    };

    auto newGame = [&]()
    {
        waitEngine();
        nodes.assign(1, Node());
        nodes[0].st.reset();
        plies.clear();
        view = 0;
        gameGen++;
        anim.active = false;
        clearSelection();
        whiteName = blackName = resultTag = "";
        youColor = 0;
        listScroll = 0;
        analyzer.setMoves({});
        analyzer.setFocus(0);
        if (playerOk)
            player.newGame();
    };

    // plays a move from the position on screen. animate=false when the piece was dragged there
    auto playMove = [&](int fr, int fc, int tr, int tc, bool animate) -> bool
    {
        const GameState &here = cur();
        if (!here.isLegal(fr, fc, tr, tc))
            return false;

        // same move as the game already has next? then just step forward
        if (view < last() && plies[view].fr == fr && plies[view].fc == fc && plies[view].tr == tr && plies[view].tc == tc)
        {
            goTo(view + 1);
            if (!animate)
                anim.active = false;
            return true;
        }

        Ply p;
        p.san = here.toSan(fr, fc, tr, tc);
        Node next;
        next.st = here;
        MoveResult res = next.st.makeMove(fr, fc, tr, tc);
        p.uci = res.uci;
        p.fr = fr; p.fc = fc; p.tr = tr; p.tc = tc;
        p.capture = res.capture;
        bool check = next.st.inCheck(next.st.turn);
        bool canMove = next.st.hasAnyLegalMove();
        next.status = check ? (canMove ? 1 : 2) : (canMove ? 0 : 3);

        if (view < last())
        {
            gameGen++;
            if (!lastPgn.empty() && mode == Mode::Analysis)
                flash("New line - press R to get the imported game back");
        }
        nodes.resize(view + 1);
        plies.resize(view);
        nodes.push_back(next);
        plies.push_back(p);

        Board before = nodes[view].st.board;
        view++;
        anim = animate ? makeAnimation(before, nodes[view].st.board) : Animation();
        clearSelection();
        badgeClock.restart();
        bannerClock.restart();
        playPlySound(view - 1);
        analyzer.setMoves(uciList());
        analyzer.setFocus(view);
        return true;
    };

    auto loadPgn = [&](const std::string &text) -> bool
    {
        PgnGame g;
        std::string err;
        if (!parsePgn(text, g, err))
        {
            flash("Import failed: " + err);
            return false;
        }
        newGame();
        mode = Mode::Analysis;
        flip = false;
        lastPgn = text;
        auto tag = [&](const char *k) { return g.tags.count(k) ? g.tags[k] : std::string(); };
        whiteName = tag("White").empty() ? "White" : tag("White");
        blackName = tag("Black").empty() ? "Black" : tag("Black");
        if (!tag("WhiteElo").empty()) whiteName += " (" + tag("WhiteElo") + ")";
        if (!tag("BlackElo").empty()) blackName += " (" + tag("BlackElo") + ")";
        resultTag = tag("Result");

        for (size_t i = 0; i < g.sans.size(); i++)
        {
            int fr, fc, tr, tc;
            if (!nodes.back().st.parseSan(g.sans[i], fr, fc, tr, tc))
            {
                flash("Stopped at move '" + g.sans[i] + "' (couldn't read it)");
                break;
            }
            Ply p;
            p.san = nodes.back().st.toSan(fr, fc, tr, tc);
            Node next;
            next.st = nodes.back().st;
            MoveResult res = next.st.makeMove(fr, fc, tr, tc);
            p.uci = res.uci;
            p.fr = fr; p.fc = fc; p.tr = tr; p.tc = tc;
            p.capture = res.capture;
            bool check = next.st.inCheck(next.st.turn);
            bool canMove = next.st.hasAnyLegalMove();
            next.status = check ? (canMove ? 1 : 2) : (canMove ? 0 : 3);
            nodes.push_back(next);
            plies.push_back(p);
        }
        view = 0;
        analyzer.setMoves(uciList());
        analyzer.setFocus(0);
        if (flashMsg.empty() || flashClock.getElapsedTime().asSeconds() > 0.1f)
            flash("Loaded " + std::to_string(plies.size()) + " moves - press → to step through");
        return true;
    };

    auto setMode = [&](Mode m)
    {
        if (m != Mode::Analysis && !playerOk)
            return;
        mode = m;
        lastPgn.clear();
        newGame();
        flip = (m == Mode::VsEngineAsBlack);
    };

    auto openFileDialog = [&]()
    {
        if (openFuture.valid())
            return;
        openFuture = std::async(std::launch::async, []()
                                {
            std::string path;
            FILE *p = popen("zenity --file-selection --title='Open a PGN game' "
                            "--file-filter='PGN games | *.pgn' --file-filter='All files | *' 2>/dev/null", "r");
            if (!p)
                return path;
            char buf[4096];
            if (fgets(buf, sizeof buf, p))
                path = buf;
            pclose(p);
            while (!path.empty() && (path.back() == '\n' || path.back() == '\r'))
                path.pop_back();
            return path; });
    };

    auto startFetch = [&]()
    {
        if (ccFuture.valid())
            return;
        if (!validUsername(userInput))
        {
            cc = ChessComFetch();
            cc.error = "Type your chess.com username first";
            return;
        }
        cc = ChessComFetch();
        ccScroll = 0;
        std::string name = userInput;
        ccFuture = std::async(std::launch::async, [name]() { return fetchPlayer(name); });
    };
    auto openGames = [&](bool fromKey)
    {
        gamesOpen = true;
        ignoreText = fromKey;
        clearSelection();
        // remembered username and nothing downloaded yet -> download right away
        if (cc.games.empty() && cc.error.empty() && validUsername(userInput))
            startFetch();
    };
    auto loadOlder = [&]()
    {
        if (ccMoreFuture.valid() || cc.nextArchive < 0)
            return;
        std::string url = cc.archives[cc.nextArchive];
        ccMoreFuture = std::async(std::launch::async, [url]()
                                  {
            std::vector<ChessComGame> games;
            std::string err;
            fetchMonth(url, games, err);
            return std::make_pair(games, err); });
    };
    auto loadChessComGame = [&](const ChessComGame &g)
    {
        if (!loadPgn(g.pgn))
            return;
        std::string me = lower(cc.username);
        youColor = lower(g.white) == me ? WHITE : lower(g.black) == me ? BLACK : 0;
        flip = (youColor == BLACK);
        gamesOpen = false;
    };

    if (argc > 1)
        loadPgn(readFile(argv[1]));

    // ------------------------------------------------------------ screen helpers
    Layout L = computeLayout(window.getSize());
    auto squareTopLeft = [&](float row, float col) // board coords -> screen pixel (works with fractions)
    {
        float dr = flip ? 7 - row : row, dc = flip ? 7 - col : col;
        return sf::Vector2f(L.boardX + dc * L.sq, L.boardY + dr * L.sq);
    };
    auto squareAt = [&](sf::Vector2f p, int &row, int &col) -> bool
    {
        int dc = int(std::floor((p.x - L.boardX) / L.sq)), dr = int(std::floor((p.y - L.boardY) / L.sq));
        if (dc < 0 || dc > 7 || dr < 0 || dr > 7)
            return false;
        row = flip ? 7 - dr : dr;
        col = flip ? 7 - dc : dc;
        return true;
    };
    auto drawPiece = [&](int piece, sf::Vector2f topLeft, std::uint8_t alpha = 255, float scale = 1.f)
    {
        sf::Sprite s(textures[piece + 6]);
        float k = L.sq * 0.88f / 60.f * scale;
        s.setScale({k, k});
        s.setOrigin({30.f, 30.f});
        s.setPosition({topLeft.x + L.sq / 2.f, topLeft.y + L.sq / 2.f});
        s.setColor(sf::Color(255, 255, 255, alpha));
        window.draw(s);
    };
    auto makeText = [&](const std::string &s, unsigned size, sf::Color color)
    {
        sf::Text t(font, sf::String::fromUtf8(s.begin(), s.end()), size);
        t.setFillColor(color);
        return t;
    };
    auto drawText = [&](const std::string &s, float x, float y, unsigned size, sf::Color color) -> sf::FloatRect
    {
        sf::Text t = makeText(s, size, color);
        t.setPosition({std::round(x), std::round(y)});
        window.draw(t);
        return t.getGlobalBounds();
    };
    auto drawCentered = [&](const std::string &s, sf::Vector2f c, unsigned size, sf::Color color)
    {
        sf::Text t = makeText(s, size, color);
        sf::FloatRect b = t.getLocalBounds();
        t.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
        t.setPosition(c);
        window.draw(t);
        return t;
    };
    auto drawArrow = [&](const std::string &uci, sf::Color color)
    {
        int fr, fc, tr, tc;
        if (!GameState::parseUci(uci, fr, fc, tr, tc))
            return;
        sf::Vector2f a = squareTopLeft(fr, fc) + sf::Vector2f(L.sq / 2, L.sq / 2);
        sf::Vector2f b = squareTopLeft(tr, tc) + sf::Vector2f(L.sq / 2, L.sq / 2);
        sf::Vector2f d = b - a;
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len < 1)
            return;
        sf::Vector2f u = d / len, n(-u.y, u.x);
        float head = L.sq * 0.38f, headW = L.sq * 0.24f, shaftW = L.sq * 0.09f;
        sf::Vector2f base = b - u * head;
        // SFML can only fill convex shapes, so: a rectangle for the shaft + a triangle for the head
        sf::ConvexShape shaft(4);
        shaft.setPoint(0, a + n * shaftW);
        shaft.setPoint(1, base + n * shaftW);
        shaft.setPoint(2, base - n * shaftW);
        shaft.setPoint(3, a - n * shaftW);
        shaft.setFillColor(color);
        window.draw(shaft);
        sf::ConvexShape tip(3);
        tip.setPoint(0, base + n * headW);
        tip.setPoint(1, b);
        tip.setPoint(2, base - n * headW);
        tip.setFillColor(color);
        window.draw(tip);
    };

    // a clickable button with a hover effect. list = where its click handler goes
    auto drawButton = [&](sf::FloatRect r, const std::string &label, unsigned size, std::vector<Button> &list,
                          std::function<void()> action)
    {
        bool hover = r.contains(mousePos);
        bool pressed = hover && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
        sf::RectangleShape b(r.size);
        b.setPosition(r.position + sf::Vector2f(0, pressed ? 1.f : 0.f));
        b.setFillColor(hover ? sf::Color(250, 250, 250) : sf::Color(228, 228, 228));
        b.setOutlineThickness(1);
        b.setOutlineColor(sf::Color(140, 140, 140));
        window.draw(b);
        drawCentered(label, r.position + r.size / 2.f + sf::Vector2f(0, pressed ? 1.f : 0.f), size, sf::Color(25, 25, 25));
        list.push_back({r, action});
    };
    // cut a UTF-8 string to at most n bytes without breaking a character
    auto shorten = [](std::string s, size_t n) -> std::string
    {
        if (s.size() <= n)
            return s;
        size_t cut = n > 3 ? n - 3 : 0;
        while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80)
            cut--;
        return s.substr(0, cut) + "...";
    };

    // ------------------------------------------------------------ main loop
    while (window.isOpen())
    {
        float dt = frameClock.restart().asSeconds();
        L = computeLayout(window.getSize());
        bool engineThinking = engineFuture.valid();
        int engineSide = engineColor();
        bool humanCanMove = cur().turn != engineSide && nodes[view].status < 2 && !engineThinking;

        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
                continue;
            }
            if (gamesOpen) // the chess.com window gets all input while it is open
            {
                if (const auto *te = event->getIf<sf::Event::TextEntered>())
                {
                    char32_t ch = te->unicode;
                    bool skip = ignoreText && (ch == 'g' || ch == 'G');
                    ignoreText = false;
                    if (!skip && ch < 128 && (std::isalnum(int(ch)) || ch == '_' || ch == '-') && userInput.size() < 30)
                        userInput += char(ch);
                }
                else if (const auto *kp = event->getIf<sf::Event::KeyPressed>())
                {
                    if (kp->code == sf::Keyboard::Key::Escape)
                        gamesOpen = false;
                    else if (kp->code == sf::Keyboard::Key::Enter)
                        startFetch();
                    else if (kp->code == sf::Keyboard::Key::Backspace && !userInput.empty())
                        userInput.pop_back();
                    else if (kp->code == sf::Keyboard::Key::V && kp->control)
                    {
                        auto u = sf::Clipboard::getString().toUtf8();
                        for (auto c : u)
                            if (std::isalnum(int(c)) || c == '_' || c == '-')
                                userInput += char(c);
                    }
                }
                else if (const auto *mb = event->getIf<sf::Event::MouseButtonPressed>())
                {
                    mousePos = window.mapPixelToCoords(mb->position);
                    for (auto &b : overlayButtons)
                        if (b.rect.contains(mousePos))
                        {
                            b.action();
                            break;
                        }
                }
                else if (const auto *mm = event->getIf<sf::Event::MouseMoved>())
                    mousePos = window.mapPixelToCoords(mm->position);
                else if (const auto *mw = event->getIf<sf::Event::MouseWheelScrolled>())
                    ccScroll -= int(mw->delta);
                else if (const auto *rs = event->getIf<sf::Event::Resized>())
                    window.setView(sf::View(sf::FloatRect({0.f, 0.f}, sf::Vector2f(rs->size))));
                continue;
            }
            if (const auto *rs = event->getIf<sf::Event::Resized>())
            {
                window.setView(sf::View(sf::FloatRect({0.f, 0.f}, sf::Vector2f(rs->size))));
                L = computeLayout(window.getSize());
            }
            else if (const auto *mm = event->getIf<sf::Event::MouseMoved>())
                mousePos = window.mapPixelToCoords(mm->position);
            else if (const auto *mb = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mb->button != sf::Mouse::Button::Left)
                    continue;
                mousePos = window.mapPixelToCoords(mb->position);

                bool hitButton = false;
                for (auto &b : buttons)
                    if (b.rect.contains(mousePos))
                    {
                        b.action();
                        hitButton = true;
                        break;
                    }
                int row, col;
                if (hitButton || !humanCanMove || !squareAt(mousePos, row, col))
                    continue;

                int piece = cur().board.at(row, col);
                bool own = piece != 0 && (piece > 0) == (cur().turn > 0);
                if (selRow != -1 && cur().isLegal(selRow, selCol, row, col))
                {
                    playMove(selRow, selCol, row, col, true);
                }
                else if (own)
                {
                    pressOnSelected = (row == selRow && col == selCol);
                    selRow = row;
                    selCol = col;
                    dragging = true;
                    anim.active = false;
                }
                else
                    clearSelection();
            }
            else if (const auto *mr = event->getIf<sf::Event::MouseButtonReleased>())
            {
                if (mr->button != sf::Mouse::Button::Left || !dragging)
                    continue;
                mousePos = window.mapPixelToCoords(mr->position);
                dragging = false;
                int row, col;
                if (!squareAt(mousePos, row, col))
                    continue;
                if ((row != selRow || col != selCol) && cur().isLegal(selRow, selCol, row, col))
                    playMove(selRow, selCol, row, col, false);
                else if (row == selRow && col == selCol && pressOnSelected)
                    clearSelection();
            }
            else if (const auto *mw = event->getIf<sf::Event::MouseWheelScrolled>())
            {
                sf::Vector2f p = window.mapPixelToCoords(mw->position);
                if (p.x >= L.panelX)
                    listScroll -= int(mw->delta);
                else if (mw->delta < 0)
                    goTo(view + 1); // scroll over the board steps through moves
                else
                    goTo(view - 1);
            }
            else if (const auto *kp = event->getIf<sf::Event::KeyPressed>())
            {
                switch (kp->code)
                {
                case sf::Keyboard::Key::Left: goTo(view - 1); break;
                case sf::Keyboard::Key::Right: goTo(view + 1); break;
                case sf::Keyboard::Key::Home: goTo(0); break;
                case sf::Keyboard::Key::End: goTo(last()); break;
                case sf::Keyboard::Key::N:
                    lastPgn.clear();
                    newGame();
                    break;
                case sf::Keyboard::Key::Num1: setMode(Mode::Analysis); break;
                case sf::Keyboard::Key::Num2: setMode(Mode::VsEngineAsWhite); break;
                case sf::Keyboard::Key::Num3: setMode(Mode::VsEngineAsBlack); break;
                case sf::Keyboard::Key::F: flip = !flip; break;
                case sf::Keyboard::Key::H: showHint = !showHint; break;
                case sf::Keyboard::Key::Up: engineLevel = std::min(20, engineLevel + 1); break;
                case sf::Keyboard::Key::Down: engineLevel = std::max(0, engineLevel - 1); break;
                case sf::Keyboard::Key::O: openFileDialog(); break;
                case sf::Keyboard::Key::G: openGames(true); break;
                case sf::Keyboard::Key::R:
                    if (!lastPgn.empty())
                    {
                        std::string pgn = lastPgn;
                        int keep = view;
                        loadPgn(pgn);
                        goTo(std::min(keep, last()));
                    }
                    break;
                case sf::Keyboard::Key::V:
                    if (kp->control)
                    {
                        auto u = sf::Clipboard::getString().toUtf8();
                        loadPgn(std::string(u.begin(), u.end()));
                    }
                    break;
                case sf::Keyboard::Key::F11:
                    fullscreen = !fullscreen;
                    createWindow();
                    break;
                case sf::Keyboard::Key::Escape:
                    if (fullscreen)
                    {
                        fullscreen = false;
                        createWindow();
                    }
                    break;
                default:
                    break;
                }
            }
        }

        // ---------- file picked in the "open" dialog ----------
        if (openFuture.valid() && openFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            std::string path = openFuture.get();
            if (!path.empty())
                loadPgn(readFile(path));
        }

        // ---------- chess.com downloads finished ----------
        if (ccFuture.valid() && ccFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            cc = ccFuture.get();
            if (cc.error.empty())
                saveUsername(cc.username);
        }
        if (ccMoreFuture.valid() && ccMoreFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            auto [games, err] = ccMoreFuture.get();
            cc.games.insert(cc.games.end(), games.begin(), games.end());
            cc.error = err;
            if (err.empty())
                cc.nextArchive--;
        }

        // ---------- Stockfish plays ----------
        engineSide = engineColor();
        if (playerOk && engineSide != 0 && view == last() && nodes[view].status < 2 &&
            cur().turn == engineSide && !engineFuture.valid())
        {
            std::vector<std::string> moves = uciList();
            int level = engineLevel;
            engineRequestGen = gameGen;
            engineFuture = std::async(std::launch::async, [&player, moves, level]()
                                      {
                player.setSkillLevel(level);
                return player.bestMove(moves, ENGINE_THINK_MS); });
        }
        if (engineFuture.valid() && engineFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            std::string best = engineFuture.get();
            engineFuture = std::future<std::string>();
            int fr, fc, tr, tc;
            // only use the answer if the game didn't change while it was thinking
            if (engineRequestGen == gameGen && view == last() && GameState::parseUci(best, fr, fc, tr, tc))
                playMove(fr, fc, tr, tc, true);
        }

        // ---------- review data for the position on screen ----------
        std::vector<Eval> evals = analyzer.snapshot();
        auto evalAt = [&](int i) { return i < int(evals.size()) ? evals[i] : Eval(); };
        auto isBook = [&](int ply)
        {
            return book.contains(nodes[ply + 1].st) && (ply == 0 || book.contains(nodes[ply].st));
        };
        auto reviewOf = [&](int ply) // grade of plies[ply]
        {
            Eval prev = ply > 0 ? evalAt(ply - 1) : Eval();
            return reviewMove(ply > 0 ? &prev : nullptr, evalAt(ply), evalAt(ply + 1),
                              ply % 2 == 0 ? WHITE : BLACK, plies[ply].uci, isBook(ply));
        };
        Eval here = evalAt(view);
        if (here.done)
            evalBarShown += (float(winPercent(here, WHITE) / 100.0) - evalBarShown) * std::min(1.f, dt * 6);
        MoveReview curReview = view > 0 ? reviewOf(view - 1) : MoveReview();
        if (curReview.grade != Grade::None && curReview.grade != Grade::Book)
            accuracyMarkerShown += (float(curReview.accuracy) - accuracyMarkerShown) * std::min(1.f, dt * 8);

        // =========================================================== drawing
        buttons.clear();
        window.clear(sf::Color(48, 46, 43));
        float boardPx = 8 * L.sq;

        // ---------- eval bar ----------
        {
            sf::RectangleShape bg({L.evalW, boardPx});
            bg.setPosition({L.evalX, L.boardY});
            bg.setFillColor(sf::Color(40, 40, 40));
            window.draw(bg);
            float whiteH = boardPx * evalBarShown;
            sf::RectangleShape w({L.evalW, whiteH});
            w.setFillColor(sf::Color(240, 240, 240));
            w.setPosition({L.evalX, flip ? L.boardY : L.boardY + boardPx - whiteH});
            window.draw(w);
            if (here.done)
            {
                std::string t = evalText(here);
                if (!here.mate)
                {
                    char buf[16];
                    snprintf(buf, sizeof buf, "%.1f", std::abs(here.cp) / 100.0);
                    t = buf;
                }
                else if (t[0] == '-')
                    t = t.substr(1);
                bool whiteAhead = here.mate ? here.winner > 0 : here.cp >= 0;
                bool atBottom = whiteAhead != flip;
                float y = atBottom ? L.boardY + boardPx - 18 : L.boardY + 4;
                drawCentered(t, {L.evalX + L.evalW / 2, y + 6}, 11,
                             whiteAhead ? sf::Color(40, 40, 40) : sf::Color(230, 230, 230));
            }
        }

        // ---------- squares + highlights ----------
        int checkRow = -1, checkCol = -1;
        if (nodes[view].status == 1 || nodes[view].status == 2)
            cur().findKing(cur().turn, checkRow, checkCol);
        const Ply *lastPly = view > 0 ? &plies[view - 1] : nullptr;

        for (int row = 0; row < 8; row++)
            for (int col = 0; col < 8; col++)
            {
                sf::RectangleShape sq({L.sq, L.sq});
                sq.setPosition(squareTopLeft(row, col));
                bool isLight = (row + col) % 2 == 0;
                sq.setFillColor(isLight ? sf::Color::White : sf::Color(0, 153, 76));
                window.draw(sq);

                sf::Color overlay = sf::Color::Transparent;
                if (lastPly && ((row == lastPly->fr && col == lastPly->fc) || (row == lastPly->tr && col == lastPly->tc)))
                    overlay = sf::Color(255, 230, 60, 110);
                if (row == selRow && col == selCol)
                    overlay = sf::Color(255, 200, 0, 150);
                if (overlay != sf::Color::Transparent)
                {
                    sq.setFillColor(overlay);
                    window.draw(sq);
                }
                if (row == checkRow && col == checkCol)
                {
                    // soft red glow under the king in check
                    for (int k = 4; k >= 1; k--)
                    {
                        sf::CircleShape glow(L.sq * 0.13f * k);
                        glow.setOrigin({glow.getRadius(), glow.getRadius()});
                        glow.setPosition(squareTopLeft(row, col) + sf::Vector2f(L.sq / 2, L.sq / 2));
                        glow.setFillColor(sf::Color(230, 30, 30, 70));
                        window.draw(glow);
                    }
                }
            }

        // coordinates in the corners of the edge squares
        for (int i = 0; i < 8; i++)
        {
            int boardCol = flip ? 7 - i : i;
            int boardRow = flip ? 7 - i : i;
            unsigned fs = unsigned(std::max(10.f, L.sq * 0.18f));
            sf::Color onLight(0, 153, 76), onDark(255, 255, 255);
            int bottomRow = flip ? 0 : 7, leftCol = flip ? 7 : 0;
            drawText(std::string(1, char('a' + boardCol)), L.boardX + i * L.sq + L.sq - fs * 0.8f,
                     L.boardY + boardPx - fs * 1.35f, fs, (bottomRow + boardCol) % 2 == 0 ? onLight : onDark);
            drawText(std::string(1, char('8' - boardRow)), L.boardX + 3, L.boardY + i * L.sq + 2, fs,
                     (boardRow + leftCol) % 2 == 0 ? onLight : onDark);
        }

        // ---------- pieces ----------
        float t = anim.active ? anim.progress() : 1.f;
        if (t >= 1.f)
            anim.active = false;
        for (int row = 0; row < 8; row++)
            for (int col = 0; col < 8; col++)
            {
                int piece = cur().board.at(row, col);
                if (piece == 0 || (anim.active && anim.hide[row][col]) || (dragging && row == selRow && col == selCol))
                    continue;
                drawPiece(piece, squareTopLeft(row, col));
            }
        if (anim.active)
        {
            float e = easeOutCubic(t);
            for (auto &f : anim.fades)
                drawPiece(f.piece, squareTopLeft(f.row, f.col), std::uint8_t(255 * (f.in ? e : 1 - e)));
            for (auto &s : anim.slides)
                drawPiece(s.piece, squareTopLeft(s.fr + (s.tr - s.fr) * e, s.fc + (s.tc - s.fc) * e));
        }

        // dots on the squares the selected piece can go to (rings on captures)
        if (selRow != -1)
            for (int row = 0; row < 8; row++)
                for (int col = 0; col < 8; col++)
                    if (cur().isLegal(selRow, selCol, row, col))
                    {
                        sf::Vector2f c = squareTopLeft(row, col) + sf::Vector2f(L.sq / 2, L.sq / 2);
                        if (cur().board.at(row, col) != 0)
                        {
                            float r = L.sq * 0.44f;
                            sf::CircleShape ring(r);
                            ring.setOrigin({r, r});
                            ring.setPosition(c);
                            ring.setFillColor(sf::Color::Transparent);
                            ring.setOutlineThickness(-L.sq * 0.07f);
                            ring.setOutlineColor(sf::Color(0, 0, 0, 70));
                            window.draw(ring);
                        }
                        else
                        {
                            float r = L.sq * 0.15f;
                            sf::CircleShape dot(r);
                            dot.setOrigin({r, r});
                            dot.setPosition(c);
                            dot.setFillColor(sf::Color(0, 0, 0, 70));
                            window.draw(dot);
                        }
                    }

        // arrows: Stockfish's better move, and the hint for the position on screen
        if (!anim.active && lastPly && curReview.grade > Grade::Best && !evalAt(view - 1).best.empty())
            drawArrow(evalAt(view - 1).best, sf::Color(40, 130, 230, 170));
        if (showHint && !anim.active && here.done && !here.best.empty())
            drawArrow(here.best, sf::Color(230, 140, 30, 170));

        // grade badge on the square the last move went to (pops in)
        if (lastPly && curReview.grade != Grade::None && !anim.active)
        {
            GradeStyle gs = gradeStyle(curReview.grade);
            float bt = std::min(1.f, badgeClock.getElapsedTime().asSeconds() / 0.3f);
            float r = L.sq * 0.17f * std::max(0.f, easeOutBack(bt));
            sf::Vector2f c = squareTopLeft(lastPly->tr, lastPly->tc) + sf::Vector2f(L.sq - L.sq * 0.12f, L.sq * 0.12f);
            sf::CircleShape badge(r);
            badge.setOrigin({r, r});
            badge.setPosition(c);
            badge.setFillColor(gs.color);
            badge.setOutlineThickness(1.5f);
            badge.setOutlineColor(sf::Color::White);
            window.draw(badge);
            if (bt > 0.5f)
                drawCentered(gs.symbol, c, unsigned(std::max(9.f, L.sq * 0.19f)), sf::Color::White);
        }
        else
            badgeClock.restart();

        // the piece you are dragging follows the mouse
        if (dragging && selRow != -1)
        {
            int row, col;
            if (squareAt(mousePos, row, col))
            {
                sf::RectangleShape hover({L.sq, L.sq});
                hover.setPosition(squareTopLeft(row, col));
                hover.setFillColor(sf::Color::Transparent);
                hover.setOutlineThickness(-3.f);
                hover.setOutlineColor(sf::Color(255, 255, 255, 180));
                window.draw(hover);
            }
            drawPiece(cur().board.at(selRow, selCol), mousePos - sf::Vector2f(L.sq / 2, L.sq / 2), 255, 1.12f);
        }

        // checkmate / stalemate banner (fades in)
        if (nodes[view].status >= 2 && !anim.active)
        {
            float bt = std::min(1.f, bannerClock.getElapsedTime().asSeconds() / 0.5f);
            sf::RectangleShape shade({boardPx, boardPx});
            shade.setPosition({L.boardX, L.boardY});
            shade.setFillColor(sf::Color(0, 0, 0, std::uint8_t(120 * bt)));
            window.draw(shade);
            bool mate = nodes[view].status == 2;
            std::string title = mate ? "Checkmate!" : "Stalemate";
            std::string sub = mate ? (cur().turn == WHITE ? "Black wins" : "White wins") : "Draw";
            sf::Vector2f c(L.boardX + boardPx / 2, L.boardY + boardPx / 2);
            float s = 0.7f + 0.3f * easeOutBack(bt);
            sf::Text tt = makeText(title, unsigned(L.sq * 0.75f), sf::Color(255, 70, 70, std::uint8_t(255 * bt)));
            tt.setOutlineColor(sf::Color(0, 0, 0, std::uint8_t(255 * bt)));
            tt.setOutlineThickness(3.f);
            sf::FloatRect b = tt.getLocalBounds();
            tt.setOrigin({b.position.x + b.size.x / 2, b.position.y + b.size.y / 2});
            tt.setPosition(c - sf::Vector2f(0, L.sq * 0.5f));
            tt.setScale({s, s});
            window.draw(tt);
            drawCentered(sub, c + sf::Vector2f(0, L.sq * 0.3f), unsigned(L.sq * 0.4f),
                         sf::Color(255, 255, 255, std::uint8_t(255 * bt)));
            drawCentered("N = new game    ← = go back", c + sf::Vector2f(0, L.sq * 0.85f), unsigned(L.sq * 0.22f),
                         sf::Color(220, 220, 220, std::uint8_t(255 * bt)));
        }

        // ---------- player names above and below the board ----------
        auto material = [&](int color)
        {
            static const int value[7] = {0, 1, 3, 3, 5, 0, 9};
            int sum = 0;
            for (int r = 0; r < 8; r++)
                for (int c = 0; c < 8; c++)
                {
                    int p = cur().board.at(r, c);
                    if (p != 0 && (p > 0) == (color > 0))
                        sum += value[std::abs(p)];
                }
            return sum;
        };
        int matDiff = material(WHITE) - material(BLACK);
        auto drawNameBar = [&](int color, float barY)
        {
            std::string name;
            if (!whiteName.empty())
                name = color == WHITE ? whiteName : blackName;
            else if (engineColor() == color)
                name = "Stockfish (level " + std::to_string(engineLevel) + ")";
            else if (mode != Mode::Analysis)
                name = "You";
            else
                name = color == WHITE ? "White" : "Black";
            float cy = barY + L.barH / 2;
            sf::RectangleShape icon({16, 16});
            icon.setOrigin({8, 8});
            icon.setPosition({L.boardX + 8, cy});
            icon.setFillColor(color == WHITE ? sf::Color(240, 240, 240) : sf::Color(20, 20, 20));
            icon.setOutlineThickness(1);
            icon.setOutlineColor(sf::Color(150, 150, 150));
            window.draw(icon);
            sf::FloatRect nb = drawText(name, L.boardX + 24, cy - 11, 16, sf::Color(235, 235, 235));
            float x = nb.position.x + nb.size.x + 10;
            if (youColor == color)
            {
                sf::FloatRect yb = drawText("(you)", x, cy - 9, 13, sf::Color(150, 205, 120));
                x = yb.position.x + yb.size.x + 10;
            }
            if (cur().turn == color && nodes[view].status < 2) // whose turn it is
            {
                sf::CircleShape dot(5);
                dot.setOrigin({5, 5});
                dot.setPosition({x + 5, cy});
                dot.setFillColor(sf::Color(120, 200, 90));
                window.draw(dot);
            }
            int adv = color == WHITE ? matDiff : -matDiff;
            if (adv > 0)
            {
                sf::Text t = makeText("+" + std::to_string(adv), 15, sf::Color(200, 200, 200));
                sf::FloatRect b = t.getLocalBounds();
                t.setPosition({std::round(L.boardX + boardPx - b.size.x - b.position.x - 4), std::round(cy - 10)});
                window.draw(t);
            }
        };
        int topColor = flip ? WHITE : BLACK;
        drawNameBar(topColor, L.boardY - L.barH);
        drawNameBar(-topColor, L.boardY + boardPx);

        // =========================================================== side panel
        float px = L.panelX, pw = L.panelW, y = L.panelY;
        float panelBottom = L.panelY + L.panelH;
        sf::RectangleShape panel({pw, L.panelH});
        panel.setPosition({px, y});
        panel.setFillColor(sf::Color(192, 192, 192));
        window.draw(panel);
        const sf::Color ink(25, 25, 25), soft(70, 70, 70);
        float ix = px + 14, iw = pw - 28;
        y += 10;

        // header: what we are doing, the opening, and the chess.com button
        {
            std::string title = !whiteName.empty()        ? "Game review"
                                : mode == Mode::Analysis ? "Analysis board"
                                                         : "You vs Stockfish";
            if (!whiteName.empty() && !resultTag.empty() && resultTag != "*")
                title += "  " + resultTag;
            drawText(title, ix, y, 15, ink);
            drawButton(sf::FloatRect({ix + iw - 128, y - 3}, {128, 26}), "chess.com (G)", 13, buttons,
                       [&]() { openGames(false); });

            std::string opening; // deepest named opening reached so far
            for (int i = view; i >= 1 && opening.empty(); i--)
                opening = book.nameOf(nodes[i].st);
            if (!opening.empty())
                drawText(shorten(opening, size_t(std::max(10.f, iw / 7.9f))), ix, y + 26, 13, soft);
            y += 50;
        }

        // ---------- the move card: how much Stockfish liked the move ----------
        {
            float cardH = 128;
            sf::RectangleShape card({iw, cardH});
            card.setPosition({ix, y});
            card.setFillColor(sf::Color(232, 232, 232));
            card.setOutlineThickness(1);
            card.setOutlineColor(sf::Color(160, 160, 160));
            window.draw(card);
            float cx = ix + 12, cy = y + 10;

            if (!analyzerOk)
                drawText("Stockfish not found -\nno review available", cx, cy, 15, soft);
            else if (view == 0)
            {
                drawText("Start position", cx, cy, 18, ink);
                drawText("Eval " + evalText(here), cx, cy + 30, 14, soft);
                drawText("→ or scroll on the board\nto step through the game", cx, cy + 56, 13, soft);
            }
            else
            {
                const Ply &p = plies[view - 1];
                int moveNo = (view + 1) / 2;
                std::string label = std::to_string(moveNo) + (view % 2 == 1 ? ". " : "... ") + p.san;
                sf::FloatRect lb = drawText(label, cx, cy - 2, 20, ink);

                if (curReview.grade == Grade::None)
                    drawText("Analyzing...", cx, cy + 32, 15, soft);
                else
                {
                    GradeStyle gs = gradeStyle(curReview.grade);
                    sf::Vector2f bc(lb.position.x + lb.size.x + 22, cy + 11);
                    sf::CircleShape badge(11);
                    badge.setOrigin({11, 11});
                    badge.setPosition(bc);
                    badge.setFillColor(gs.color);
                    window.draw(badge);
                    drawCentered(gs.symbol, bc, 12, sf::Color::White);
                    sf::FloatRect gb = drawText(gs.name, bc.x + 18, cy + 1, 16, gs.color);
                    // big mistakes get a second, longer look before the grade is final
                    if (curReview.grade >= Grade::Miss && curReview.drop >= CHECK_DROP &&
                        (!evalAt(view - 1).deep || !evalAt(view).deep))
                        drawText("checking...", gb.position.x + gb.size.x + 8, cy + 3, 12, soft);
                }
                if (curReview.grade == Grade::Book)
                {
                    drawText("Known opening theory", cx, cy + 34, 14, ink);
                    drawText("Eval " + evalText(here), cx, cy + 58, 14, soft);
                }
                else if (curReview.grade != Grade::None)
                {
                    // the scale: red (bad) -> yellow -> green (Stockfish loved it)
                    float sx = cx, sy = cy + 34, sw = iw - 24, sh = 14;
                    sf::VertexArray grad(sf::PrimitiveType::TriangleStrip);
                    const std::pair<float, sf::Color> stops[] = {{0.f, sf::Color(205, 45, 45)},
                                                                 {0.45f, sf::Color(235, 120, 40)},
                                                                 {0.7f, sf::Color(225, 195, 40)},
                                                                 {1.f, sf::Color(86, 160, 60)}};
                    for (auto &[f, col] : stops)
                    {
                        grad.append(sf::Vertex{{sx + sw * f, sy}, col});
                        grad.append(sf::Vertex{{sx + sw * f, sy + sh}, col});
                    }
                    window.draw(grad);
                    float mx = sx + sw * accuracyMarkerShown / 100.f;
                    sf::ConvexShape marker(3);
                    marker.setPoint(0, {mx - 7, sy - 8});
                    marker.setPoint(1, {mx + 7, sy - 8});
                    marker.setPoint(2, {mx, sy + 2});
                    marker.setFillColor(ink);
                    window.draw(marker);
                    sf::RectangleShape tick({3, sh + 4});
                    tick.setPosition({mx - 1.5f, sy - 2});
                    tick.setFillColor(ink);
                    window.draw(tick);

                    char buf[64];
                    snprintf(buf, sizeof buf, "Stockfish liked it: %d/100", int(std::round(curReview.accuracy)));
                    drawText(buf, cx, sy + sh + 8, 14, ink);

                    std::string line = "Eval " + evalText(here);
                    if (curReview.grade != Grade::Best && !evalAt(view - 1).best.empty())
                    {
                        // turn Stockfish's best move into SAN in the position before
                        int fr, fc, tr, tc;
                        if (GameState::parseUci(evalAt(view - 1).best, fr, fc, tr, tc))
                            line += "   Best: " + nodes[view - 1].st.toSan(fr, fc, tr, tc);
                    }
                    drawText(line, cx, sy + sh + 30, 14, soft);
                }
            }
            y += cardH + 10;
        }

        // ---------- accuracy of both players ----------
        if (analyzerOk && !plies.empty())
        {
            double sum[2] = {0, 0};
            int cnt[2] = {0, 0}, done = 0;
            for (int i = 0; i < last(); i++)
            {
                MoveReview r = reviewOf(i);
                if (r.grade == Grade::None)
                    continue;
                sum[i % 2] += r.accuracy;
                cnt[i % 2]++;
            }
            for (auto &e : evals)
                done += e.done;
            char buf[96];
            snprintf(buf, sizeof buf, "Accuracy  W %.1f  B %.1f", cnt[0] ? sum[0] / cnt[0] : 0.0,
                     cnt[1] ? sum[1] / cnt[1] : 0.0);
            drawText(buf, ix, y, 14, ink);
            int pending = analyzer.pendingChecks();
            if (done == int(evals.size()) && pending > 0)
                drawText("double-checking " + std::to_string(pending) + " big mistake" + (pending > 1 ? "s" : "") + "...",
                         ix, y + 18, 12, soft);
            if (done < int(evals.size()))
            {
                float f = float(done) / evals.size();
                sf::RectangleShape bar({iw, 4});
                bar.setPosition({ix, y + 20});
                bar.setFillColor(sf::Color(160, 160, 160));
                window.draw(bar);
                bar.setSize({iw * f, 4});
                bar.setFillColor(sf::Color(40, 130, 230));
                window.draw(bar);
            }
            y += 36;
        }

        // ---------- move list ----------
        float bottomH = 150;
        float listTop = y, listBottom = panelBottom - bottomH;
        float rowH = 22;
        int visibleRows = std::max(1, int((listBottom - listTop) / rowH));
        int totalRows = (last() + 1) / 2;
        if (view != lastScrolledView) // keep the current move visible
        {
            int curRow = view > 0 ? (view - 1) / 2 : 0;
            if (curRow < listScroll)
                listScroll = curRow;
            if (curRow >= listScroll + visibleRows)
                listScroll = curRow - visibleRows + 1;
            lastScrolledView = view;
        }
        listScroll = std::clamp(listScroll, 0, std::max(0, totalRows - visibleRows));
        float colW = (iw - 44) / 2;
        for (int r = listScroll; r < totalRows && r < listScroll + visibleRows; r++)
        {
            float ry = listTop + (r - listScroll) * rowH;
            if (r % 2 == 1)
            {
                sf::RectangleShape stripe({iw, rowH});
                stripe.setPosition({ix, ry});
                stripe.setFillColor(sf::Color(182, 182, 182));
                window.draw(stripe);
            }
            drawText(std::to_string(r + 1) + ".", ix + 4, ry + 2, 14, soft);
            for (int side = 0; side < 2; side++)
            {
                int ply = r * 2 + side;
                if (ply >= last())
                    break;
                sf::FloatRect cell({ix + 40 + side * colW, ry}, {colW - 4, rowH});
                bool isCurrent = (ply == view - 1);
                bool hovered = cell.contains(mousePos);
                if (isCurrent || hovered)
                {
                    sf::RectangleShape hl(cell.size);
                    hl.setPosition(cell.position);
                    hl.setFillColor(isCurrent ? sf::Color(250, 250, 250) : sf::Color(215, 215, 215));
                    window.draw(hl);
                }
                sf::FloatRect sb = drawText(plies[ply].san, cell.position.x + 6, ry + 2, 15, ink);
                MoveReview mr = reviewOf(ply);
                if (mr.grade == Grade::Book || mr.grade == Grade::Best || mr.grade >= Grade::Miss)
                {
                    GradeStyle gs = gradeStyle(mr.grade);
                    drawText(gs.symbol, sb.position.x + sb.size.x + 4, ry + 2, 14, gs.color);
                }
                buttons.push_back({cell, [&, ply]() { goTo(ply + 1); }});
            }
        }
        if (plies.empty())
            drawText("No moves yet.\n\nCtrl+V  paste a PGN\nO       open a .pgn file", ix + 4, listTop + 4, 14, soft);

        // ---------- navigation buttons ----------
        {
            float by = panelBottom - bottomH + 8, bh = 38;
            const char *labels[5] = {"⏮", "◀", "▶", "⏭", "Flip"};
            std::function<void()> actions[5] = {
                [&]() { goTo(0); }, [&]() { goTo(view - 1); }, [&]() { goTo(view + 1); },
                [&]() { goTo(last()); }, [&]() { flip = !flip; }};
            float gap = 6, bw = (iw - 4 * gap) / 5;
            for (int i = 0; i < 5; i++)
                drawButton(sf::FloatRect({ix + i * (bw + gap), by}, {bw, bh}), labels[i], i == 4 ? 14 : 18, buttons,
                           actions[i]);

            // status line
            std::string status;
            sf::Color sc = ink;
            int st = nodes[view].status;
            if (!flashMsg.empty() && flashClock.getElapsedTime().asSeconds() < 5)
                status = flashMsg, sc = sf::Color(20, 80, 170);
            else if (engineFuture.valid())
                status = "Stockfish is thinking" + std::string(1 + int(flashClock.getElapsedTime().asSeconds() * 3) % 3, '.');
            else if (openFuture.valid())
                status = "Choose a PGN file...";
            else if (st == 2)
                status = std::string("Checkmate - ") + (cur().turn == WHITE ? "Black" : "White") + " wins", sc = sf::Color(180, 0, 0);
            else if (st == 3)
                status = "Stalemate - draw";
            else
            {
                status = cur().turn == WHITE ? "White to move" : "Black to move";
                if (st == 1)
                    status += "  CHECK!", sc = sf::Color(180, 0, 0);
                if (view != last())
                    status += "  (move " + std::to_string(view) + "/" + std::to_string(last()) + ")";
            }
            drawText(status, ix, by + bh + 8, 14, sc);

            const char *help[] = {"←→ Home End  step   H  hint arrow",
                                  "G chess.com games  Ctrl+V paste PGN",
                                  "O open .pgn  N new  1 analysis",
                                  "2/3 play vs SF  F flip  F11 fullscreen"};
            for (int i = 0; i < 4; i++)
                drawText(help[i], ix, by + bh + 30 + i * 16, 12, soft);
        }

        // =========================================================== chess.com games window
        overlayButtons.clear();
        if (gamesOpen)
        {
            sf::RectangleShape dim({L.W, L.H});
            dim.setFillColor(sf::Color(0, 0, 0, 170));
            window.draw(dim);
            float bw = std::min(800.f, L.W - 40), bh = L.H - 60;
            float bx = std::floor((L.W - bw) / 2), by = 30;
            sf::RectangleShape box({bw, bh});
            box.setPosition({bx, by});
            box.setFillColor(sf::Color(232, 232, 232));
            box.setOutlineThickness(1);
            box.setOutlineColor(sf::Color(120, 120, 120));
            window.draw(box);

            drawText("chess.com games", bx + 20, by + 16, 22, ink);
            drawButton(sf::FloatRect({bx + bw - 48, by + 12}, {34, 32}), "✕", 16, overlayButtons,
                       [&]() { gamesOpen = false; });

            // username field (always focused - just type)
            float fy = by + 62;
            drawText("Username", bx + 20, fy + 7, 15, soft);
            sf::RectangleShape field({270, 34});
            field.setPosition({bx + 112, fy});
            field.setFillColor(sf::Color::White);
            field.setOutlineThickness(2);
            field.setOutlineColor(sf::Color(40, 130, 230));
            window.draw(field);
            bool blink = int(frameClockTotal.getElapsedTime().asSeconds() * 2) % 2 == 0;
            drawText(userInput + (blink ? "|" : " "), bx + 120, fy + 6, 17, ink);
            drawButton(sf::FloatRect({bx + 392, fy}, {90, 34}), ccFuture.valid() ? "..." : "Load", 15, overlayButtons,
                       [&]() { startFetch(); });

            std::string st;
            sf::Color stc = soft;
            if (ccFuture.valid())
                st = "Downloading games from chess.com...";
            else if (!cc.error.empty())
                st = cc.error, stc = sf::Color(180, 30, 30);
            else if (!cc.games.empty())
                st = std::to_string(cc.games.size()) + " games of " + cc.username + ", newest first - click one to review it";
            else
                st = "Type your chess.com username and press Enter";
            drawText(st, bx + 20, fy + 46, 14, stc);

            // the games
            float ly = fy + 78, lb = by + bh - 58, rh = 50;
            int n = int(cc.games.size());
            int vis = std::max(1, int((lb - ly) / rh));
            ccScroll = std::clamp(ccScroll, 0, std::max(0, n - vis));
            std::string me = lower(cc.username);
            for (int i = ccScroll; i < n && i < ccScroll + vis; i++)
            {
                const ChessComGame &g = cc.games[i];
                float ry = ly + (i - ccScroll) * rh;
                sf::FloatRect row({bx + 16, ry}, {bw - 32, rh - 5});
                sf::RectangleShape rr(row.size);
                rr.setPosition(row.position);
                rr.setFillColor(row.contains(mousePos) ? sf::Color(252, 252, 252) : sf::Color(214, 214, 214));
                window.draw(rr);

                int mine = lower(g.white) == me ? WHITE : lower(g.black) == me ? BLACK : 0;
                std::string res = g.result;
                sf::Color rc(130, 130, 130);
                if (g.result == "1/2-1/2")
                    res = "=";
                else if ((g.result == "1-0" && mine == WHITE) || (g.result == "0-1" && mine == BLACK))
                    res = "W", rc = sf::Color(86, 160, 60);
                else if (mine != 0)
                    res = "L", rc = sf::Color(205, 45, 45);
                sf::RectangleShape rb({34, 34});
                rb.setPosition({row.position.x + 6, ry + 5.5f});
                rb.setFillColor(rc);
                window.draw(rb);
                drawCentered(res, rb.getPosition() + sf::Vector2f(17, 17), 16, sf::Color::White);

                size_t maxChars = size_t(std::max(20.f, (row.size.x - 60) / 7.3f));
                drawText(shorten("○ " + g.white + " (" + g.whiteElo + ")   ● " + g.black + " (" + g.blackElo + ")",
                                 maxChars),
                         row.position.x + 50, ry + 4, 15, ink);
                std::string info = g.date + " · " + g.timeControl + (g.opening.empty() ? "" : " · " + g.opening);
                drawText(shorten(info, maxChars), row.position.x + 50, ry + 25, 12, soft);
                overlayButtons.push_back({row, [&, i]() { loadChessComGame(cc.games[i]); }});
            }
            if (n > vis)
                drawText("scroll for more  (" + std::to_string(ccScroll + 1) + "-" +
                             std::to_string(std::min(n, ccScroll + vis)) + " of " + std::to_string(n) + ")",
                         bx + bw - 300, by + bh - 40, 13, soft);
            if (cc.nextArchive >= 0 && !cc.games.empty())
                drawButton(sf::FloatRect({bx + 16, by + bh - 48}, {210, 34}),
                           ccMoreFuture.valid() ? "Loading..." : "Load older games", 14, overlayButtons,
                           [&]() { loadOlder(); });

            // clicks inside the box do nothing, clicks outside close it
            overlayButtons.push_back({sf::FloatRect({bx, by}, {bw, bh}), []() {}});
            overlayButtons.push_back({sf::FloatRect({0, 0}, {L.W, L.H}), [&]() { gamesOpen = false; }});
        }

        window.display();
    }

    waitEngine();
    return 0;
}
