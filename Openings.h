#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "Gamestate.h"
#include "Pgn.h"

class OpeningBook
{
public:
    bool load(const std::string &path)
    {
        std::ifstream in(path);
        if (!in)
            return false;
        std::string line;
        std::getline(in, line); // header: eco  name  pgn
        while (std::getline(in, line))
        {
            std::stringstream ss(line);
            std::string eco, name, moves;
            if (!std::getline(ss, eco, '\t') || !std::getline(ss, name, '\t') || !std::getline(ss, moves))
                continue;
            PgnGame g;
            std::string err;
            if (!parsePgn(moves, g, err))
                continue;
            GameState st;
            st.reset();
            bool ok = true;
            for (const std::string &san : g.sans)
            {
                int fr, fc, tr, tc;
                if (!st.parseSan(san, fr, fc, tr, tc))
                {
                    ok = false;
                    break;
                }
                st.makeMove(fr, fc, tr, tc);
                positions.insert(key(st));
            }
            if (ok)
                names.emplace(key(st), name);
        }
        return !positions.empty();
    }

    bool contains(const GameState &st) const { return positions.count(key(st)) > 0; }

    // name of the opening if this exact position ends a named line, else ""
    std::string nameOf(const GameState &st) const
    {
        auto it = names.find(key(st));
        return it == names.end() ? std::string() : it->second;
    }

    static std::string key(const GameState &st)
    {
        std::string k(65, ' ');
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                k[r * 8 + c] = char('0' + st.board.at(r, c) + 6);
        k[64] = st.turn == WHITE ? 'w' : 'b';
        return k;
    }

private:
    std::unordered_set<std::string> positions;
    std::unordered_map<std::string, std::string> names;
};
