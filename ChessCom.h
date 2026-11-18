#pragma once
// Downloads a player's games from the public chess.com API (no login needed).
// Uses the `curl` command, so nothing extra has to be linked.
//https://api.chess.com/pub/player/<user>/games/archives =list of months
//https://api.chess.com/pub/player/<user>/games/2026/11/pgn=all games of that month as PGN
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "Pgn.h"

struct ChessComGame
{
    std::string pgn;
    std::string white, black, whiteElo, blackElo, result; // result: "1-0", "0-1", "1/2-1/2"
    std::string date, time;                               // "2026.11.17", "04:58:29" (UTC)
    std::string timeControl;                              // "3 min", "10|5", "daily"
    std::string opening;
};

struct ChessComFetch
{
    std::string username;
    std::vector<std::string> archives; // month URLs oldest first
    int nextArchive = -1;              // the next older month to load, -1 = none left
    std::vector<ChessComGame> games;   // newest first
    std::string error;
};

// chess.com usernames: letters, digits, '_' and '-'. Checking this also keeps the curl command safe.
inline bool validUsername(const std::string &u)
{
    if (u.empty() || u.size() > 30)
        return false;
    for (char c : u)
        if (!std::isalnum((unsigned char)c) && c != '_' && c != '-')
            return false;
    return true;
}

//get a URL with curl. httpCode = 0 if curl itself failed 
inline std::string httpGet(const std::string &url, int &httpCode)
{
    httpCode = 0;
    // the URL only ever contains [a-z0-9_-/.:] (username is validated), so single quotes are safe
    std::string cmd = "curl -sL --max-time 25 -A 'Chess-gui-cpp' -w '\\n%{http_code}' '" + url + "' 2>/dev/null";
    FILE *p = popen(cmd.c_str(), "r");
    if (!p)
        return "";
    std::string out;
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, p)) > 0)
        out.append(buf, n);
    pclose(p);
    size_t nl = out.rfind('\n');
    if (nl == std::string::npos)
        return "";
    httpCode = std::atoi(out.c_str() + nl + 1);
    out.resize(nl);
    return out;
}

// splits a file with many games into one string per game
inline std::vector<std::string> splitPgnGames(const std::string &text)
{
    std::vector<std::string> games;
    size_t start = std::string::npos, pos = 0;
    bool inMoves = false;
    while (pos < text.size())
    {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(pos, end - pos);
        bool isTag = !line.empty() && line[0] == '[';
        if (isTag && (start == std::string::npos || inMoves))
        {
            if (start != std::string::npos)
                games.push_back(text.substr(start, pos - start));
            start = pos;
            inMoves = false;
        }
        else if (!isTag && line.find_first_not_of(" \t\r") != std::string::npos)
            inMoves = true;
        pos = end + 1;
    }
    if (start != std::string::npos)
        games.push_back(text.substr(start));
    return games;
}

inline std::string niceTimeControl(const std::string &tc)
{
    if (tc.find('/') != std::string::npos)
        return "daily";
    int base = std::atoi(tc.c_str());
    size_t plus = tc.find('+');
    int inc = plus == std::string::npos ? 0 : std::atoi(tc.c_str() + plus + 1);
    std::string b = base % 60 == 0 ? std::to_string(base / 60) : std::to_string(base) + "s";
    return inc ? b + "|" + std::to_string(inc) : b + " min";
}

// "https://www.chess.com/openings/Sicilian-Defense-Najdorf-Variation-6.Be3" -> "Sicilian Defense Najdorf Variation" 'ex'
inline std::string openingFromUrl(const std::string &url)
{
    size_t p = url.find("/openings/");
    if (p == std::string::npos)
        return "";
    std::string s = url.substr(p + 10);
    size_t cut = s.find("...");
    for (size_t i = 0; i + 1 < s.size(); i++)
        if (s[i] == '-' && std::isdigit((unsigned char)s[i + 1]))
        {
            cut = std::min(cut, i);
            break;
        }
    if (cut != std::string::npos)
        s = s.substr(0, cut);
    std::replace(s.begin(), s.end(), '-', ' ');
    return s;
}

// downloads one month (archive URL) and adds its games, newest first
inline bool fetchMonth(const std::string &archiveUrl, std::vector<ChessComGame> &out, std::string &error)
{
    int code;
    std::string text = httpGet(archiveUrl + "/pgn", code);
    if (code != 200)
    {
        error = code == 0 ? "No internet connection (or curl is missing)" : "chess.com answered " + std::to_string(code);
        return false;
    }
    std::vector<ChessComGame> month;
    for (const std::string &g : splitPgnGames(text))
    {
        PgnGame pg;
        std::string err;
        if (!parsePgn(g, pg, err)) // skips variants like Chess960 too
            continue;
        if (pg.tags.count("Variant"))
            continue;
        ChessComGame c;
        c.pgn = g;
        c.white = pg.tags["White"];
        c.black = pg.tags["Black"];
        c.whiteElo = pg.tags["WhiteElo"];
        c.blackElo = pg.tags["BlackElo"];
        c.result = pg.tags["Result"];
        c.date = pg.tags.count("UTCDate") ? pg.tags["UTCDate"] : pg.tags["Date"];
        c.time = pg.tags.count("UTCTime") ? pg.tags["UTCTime"] : pg.tags["StartTime"];
        c.timeControl = niceTimeControl(pg.tags["TimeControl"]);
        c.opening = openingFromUrl(pg.tags["ECOUrl"]);
        month.push_back(c);
    }
    std::sort(month.begin(), month.end(), [](const ChessComGame &a, const ChessComGame &b)
              { return a.date + a.time > b.date + b.time; });
    out.insert(out.end(), month.begin(), month.end());
    return true;
}

// first download: the list of months + the newest month (and the one before if it has few games)
inline ChessComFetch fetchPlayer(std::string username)
{
    ChessComFetch f;
    std::transform(username.begin(), username.end(), username.begin(), ::tolower);
    f.username = username;
    if (!validUsername(username))
    {
        f.error = "That isn't a valid chess.com username";
        return f;
    }
    int code;
    std::string json = httpGet("https://api.chess.com/pub/player/" + username + "/games/archives", code);
    if (code == 404)
    {
        f.error = "No chess.com player called '" + username + "'";
        return f;
    }
    if (code != 200)
    {
        f.error = code == 0 ? "No internet connection (or curl is missing)" : "chess.com answered " + std::to_string(code);
        return f;
    }
    // the JSON is just {"archives":["https://...","https://..."]} - pull out the URLs
    size_t pos = 0;
    while ((pos = json.find("\"https://", pos)) != std::string::npos)
    {
        size_t end = json.find('"', pos + 1);
        if (end == std::string::npos)
            break;
        f.archives.push_back(json.substr(pos + 1, end - pos - 1));
        pos = end + 1;
    }
    if (f.archives.empty())
    {
        f.error = username + " has no games on chess.com";
        return f;
    }
    f.nextArchive = int(f.archives.size()) - 1;
    for (int months = 0; months < 3 && f.nextArchive >= 0 && f.games.size() < 20; months++)
    {
        if (!fetchMonth(f.archives[f.nextArchive], f.games, f.error))
            break;
        f.nextArchive--;
    }
    return f;
}
