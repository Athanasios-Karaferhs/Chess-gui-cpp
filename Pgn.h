#pragma once
// Reads a PGN (the format chess.com / lichess export games in) into a list of SAN moves.
// Comments {..}, variations (..), clock tags [%clk ..] and annotations like $1 are skipped.
// If the text contains several games, only the first one is read.
#include <cctype>
#include <map>
#include <string>
#include <vector>

struct PgnGame
{
    std::map<std::string, std::string> tags; // White, Black, WhiteElo, Result, Date ...
    std::vector<std::string> sans;           // "e4", "e5", "Nf3", ...
};

inline bool parsePgn(const std::string &text, PgnGame &game, std::string &error)
{
    game = PgnGame();
    size_t i = 0, n = text.size();
    bool movesStarted = false;

    while (i < n)
    {
        char c = text[i];
        if (std::isspace((unsigned char)c))
        {
            i++;
            continue;
        }
        if (c == '[') // tag pair:  [White "Magnus"]
        {
            if (movesStarted)
                break; // next game begins
            size_t end = text.find(']', i);
            if (end == std::string::npos)
                break;
            std::string tag = text.substr(i + 1, end - i - 1);
            size_t sp = tag.find(' ');
            size_t q1 = tag.find('"'), q2 = tag.rfind('"');
            if (sp != std::string::npos && q1 != std::string::npos && q2 > q1)
                game.tags[tag.substr(0, sp)] = tag.substr(q1 + 1, q2 - q1 - 1);
            i = end + 1;
            continue;
        }
        if (c == '{') // comment
        {
            size_t end = text.find('}', i);
            i = (end == std::string::npos) ? n : end + 1;
            continue;
        }
        if (c == ';') // comment until end of line
        {
            size_t end = text.find('\n', i);
            i = (end == std::string::npos) ? n : end + 1;
            continue;
        }
        if (c == '(') // variation, can be nested
        {
            int depth = 0;
            for (; i < n; i++)
            {
                if (text[i] == '{')
                {
                    size_t end = text.find('}', i);
                    i = (end == std::string::npos) ? n - 1 : end;
                }
                else if (text[i] == '(')
                    depth++;
                else if (text[i] == ')' && --depth == 0)
                {
                    i++;
                    break;
                }
            }
            continue;
        }

        // a normal token
        size_t j = i;
        while (j < n && !std::isspace((unsigned char)text[j]) && text[j] != '{' && text[j] != '(' &&
               text[j] != ';' && text[j] != '[')
            j++;
        std::string tok = text.substr(i, j - i);
        i = j;

        if (tok == "1-0" || tok == "0-1" || tok == "1/2-1/2" || tok == "*")
        {
            if (movesStarted)
                break;
            continue;
        }
        if (tok[0] == '$') // annotation glyph
            continue;

        // "12." / "12..." / "12.e4" -> drop the move number
        size_t k = 0;
        while (k < tok.size() && std::isdigit((unsigned char)tok[k]))
            k++;
        if (k > 0 && k < tok.size() && tok[k] == '.')
        {
            while (k < tok.size() && tok[k] == '.')
                k++;
            tok = tok.substr(k);
        }
        else if (k == tok.size()) // just a number
            continue;
        if (tok.empty())
            continue;

        game.sans.push_back(tok);
        movesStarted = true;
    }

    if (game.tags.count("SetUp") && game.tags["SetUp"] == "1")
    {
        error = "Games from a custom position aren't supported";
        return false;
    }
    if (game.sans.empty())
    {
        error = "No moves found in the PGN";
        return false;
    }
    return true;
}
