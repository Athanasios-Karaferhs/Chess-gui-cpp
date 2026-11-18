#pragma once
#include <cctype>
#include <string>
#include <vector>
#include "Board.h"
#include "Rules.h"
#define WHITE 1
#define BLACK -1

struct MoveResult
{
    bool ok = false;        // was the move legal and played?
    bool capture = false;
    bool castle = false;
    bool enPassant = false;
    bool promotion = false;
    std::string uci;        // e.g. "e2e4", "e7e8q"
};

// Holds everything needed to know the rules of a position:
// board, whose turn, castling rights and the en passant square.
class GameState
{
public:
    Board board;
    int turn = WHITE;
    castling castle;
    int epRow = -1, epCol = -1; // square a pawn may capture onto en passant

    void reset()
    {
        board.setStartingPosition();
        turn = WHITE;
        castle.reset();
        epRow = epCol = -1;
    }

    // is (row,col) attacked by any piece of colour byColor?
    bool isSquareAttacked(int row, int col, int byColor) const
    {
        for (int i = 0; i < 8; i++)
            for (int j = 0; j < 8; j++)
            {
                int piece = board.at(i, j);
                if (piece == 0 || (piece > 0) != (byColor > 0))
                    continue;
                if (i == row && j == col)
                    continue;

                switch (std::abs(piece))
                {
                case 1: if (pawn.attacks(row, col, i, j, piece)) return true; break;
                case 2: if (horse.check_horse(row, col, i, j, piece, board)) return true; break;
                case 3: if (bis.check_bis(row, col, i, j, piece, board)) return true; break;
                case 4: if (rook.check_rook(row, col, i, j, piece, board)) return true; break;
                case 5: if (king.check_king(row, col, i, j, piece, board)) return true; break;
                case 6: if (queen.check_queen(row, col, i, j, piece, board)) return true; break;
                }
            }
        return false;
    }

    bool findKing(int color, int &kRow, int &kCol) const
    {
        for (int i = 0; i < 8; i++)
            for (int j = 0; j < 8; j++)
                if (board.at(i, j) == 5 * color)
                {
                    kRow = i;
                    kCol = j;
                    return true;
                }
        return false;
    }

    bool inCheck(int color) const
    {
        int kRow, kCol;
        if (!findKing(color, kRow, kCol))
            return false;
        return isSquareAttacked(kRow, kCol, -color);
    }

    // full legality: right colour, piece can move there, and own king is not left in check
    bool isLegal(int fromRow, int fromCol, int toRow, int toCol) const
    {
        if (!isPseudoLegal(fromRow, fromCol, toRow, toCol))
            return false;
        GameState copy = *this;
        copy.applyMove(fromRow, fromCol, toRow, toCol, 'q');
        return !copy.inCheck(turn);
    }

    bool hasAnyLegalMove() const
    {
        for (int fr = 0; fr < 8; fr++)
            for (int fc = 0; fc < 8; fc++)
            {
                int piece = board.at(fr, fc);
                if (piece == 0 || (piece > 0) != (turn > 0))
                    continue;
                for (int tr = 0; tr < 8; tr++)
                    for (int tc = 0; tc < 8; tc++)
                        if (isLegal(fr, fc, tr, tc))
                            return true;
            }
        return false;
    }

    bool isCheckmate() const { return inCheck(turn) && !hasAnyLegalMove(); }
    bool isStalemate() const { return !inCheck(turn) && !hasAnyLegalMove(); }

    // plays the move if it is legal. Pawns ALWAYS promote to a queen.
    MoveResult makeMove(int fromRow, int fromCol, int toRow, int toCol)
    {
        MoveResult res;
        if (!isLegal(fromRow, fromCol, toRow, toCol))
            return res;
        res = applyMove(fromRow, fromCol, toRow, toCol, 'q');
        res.uci = squareName(fromRow, fromCol) + squareName(toRow, toCol) + (res.promotion ? "q" : "");
        return res;
    }

    // "e7e2" -> squares. Any promotion letter is ignored (always queen).
    static bool parseUci(const std::string &uci, int &fromRow, int &fromCol, int &toRow, int &toCol)
    {
        if (uci.size() < 4)
            return false;
        fromCol = uci[0] - 'a';
        fromRow = '8' - uci[1];
        toCol = uci[2] - 'a';
        toRow = '8' - uci[3];
        return fromRow >= 0 && fromRow <= 7 && fromCol >= 0 && fromCol <= 7 &&
               toRow >= 0 && toRow <= 7 && toCol >= 0 && toCol <= 7;
    }

    // Standard Algebraic Notation of a legal move, e.g. "Nf3", "exd5", "O-O", "e8=Q+", "Qh4#".
    // Call it BEFORE the move is played.
    std::string toSan(int fromRow, int fromCol, int toRow, int toCol) const
    {
        int piece = board.at(fromRow, fromCol);
        int type = std::abs(piece);
        std::string san;

        if (isCastleMove(fromRow, fromCol, toRow, toCol))
            san = toCol > fromCol ? "O-O" : "O-O-O";
        else
        {
            bool capture = board.at(toRow, toCol) != 0 ||
                           (type == 1 && fromCol != toCol); // pawn diagonal = capture (also en passant)
            if (type == 1)
            {
                if (capture)
                    san += char('a' + fromCol);
            }
            else
            {
                san += " PNBRKQ"[type];
                // disambiguation: another piece of the same kind that can also go there
                bool clash = false, sameCol = false, sameRow = false;
                for (int r = 0; r < 8; r++)
                    for (int c = 0; c < 8; c++)
                        if ((r != fromRow || c != fromCol) && board.at(r, c) == piece &&
                            isLegal(r, c, toRow, toCol))
                        {
                            clash = true;
                            sameCol |= (c == fromCol);
                            sameRow |= (r == fromRow);
                        }
                if (clash)
                {
                    if (!sameCol)
                        san += char('a' + fromCol);
                    else if (!sameRow)
                        san += char('8' - fromRow);
                    else
                        san += squareName(fromRow, fromCol);
                }
            }
            if (capture)
                san += 'x';
            san += squareName(toRow, toCol);
            if (type == 1 && (toRow == 0 || toRow == 7))
                san += "=Q";
        }

        GameState after = *this;
        after.applyMove(fromRow, fromCol, toRow, toCol, 'q');
        if (after.inCheck(after.turn))
            san += after.hasAnyLegalMove() ? "+" : "#";
        return san;
    }

    // finds the legal move described by a SAN string like "Nbd7", "exd5", "O-O", "e8=Q+"
    bool parseSan(std::string san, int &fromRow, int &fromCol, int &toRow, int &toCol) const
    {
        // strip check/mate/annotation symbols
        while (!san.empty() && std::string("+#!?").find(san.back()) != std::string::npos)
            san.pop_back();
        for (char &ch : san)
            if (ch == '0')
                ch = 'O'; // "0-0" is sometimes written instead of "O-O"

        int homeRow = (turn == WHITE) ? 7 : 0;
        if (san == "O-O" || san == "O-O-O")
        {
            fromRow = toRow = homeRow;
            fromCol = 4;
            toCol = (san == "O-O") ? 6 : 2;
            return isLegal(fromRow, fromCol, toRow, toCol);
        }

        // promotion part: "e8=Q" or "e8Q" (any piece -> we always make a queen)
        size_t eq = san.find('=');
        if (eq != std::string::npos)
            san = san.substr(0, eq);
        else if (san.size() >= 3 && std::string("QRBN").find(san.back()) != std::string::npos &&
                 std::isdigit((unsigned char)san[san.size() - 2]))
            san.pop_back();

        if (san.size() < 2)
            return false;
        int type = 1;
        size_t pos = 0;
        std::string letters = "PNBRKQ";
        if (std::isupper((unsigned char)san[0]) && letters.find(san[0]) != std::string::npos)
        {
            type = int(letters.find(san[0])) + 1;
            pos = 1;
        }

        toCol = san[san.size() - 2] - 'a';
        toRow = '8' - san[san.size() - 1];
        if (toCol < 0 || toCol > 7 || toRow < 0 || toRow > 7)
            return false;

        // whatever is between the piece letter and the target square: disambiguation and 'x'
        int needCol = -1, needRow = -1;
        for (size_t i = pos; i + 2 < san.size(); i++)
        {
            char ch = san[i];
            if (ch >= 'a' && ch <= 'h')
                needCol = ch - 'a';
            else if (ch >= '1' && ch <= '8')
                needRow = '8' - ch;
        }
        if (type == 1 && needCol == -1)
            needCol = toCol; // a pawn that doesn't capture stays on its file

        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
            {
                if (board.at(r, c) != type * turn)
                    continue;
                if ((needCol != -1 && c != needCol) || (needRow != -1 && r != needRow))
                    continue;
                if (isLegal(r, c, toRow, toCol))
                {
                    fromRow = r;
                    fromCol = c;
                    return true;
                }
            }
        return false;
    }

    static std::string squareName(int row, int col)
    {
        return std::string(1, char('a' + col)) + std::string(1, char('8' - row));
    }

private:
    pawnmove pawn;
    rookmove rook;
    horsemove horse;
    kingmove king;
    bisopmove bis;
    queenmove queen;

    bool isCastleMove(int fromRow, int fromCol, int toRow, int toCol) const
    {
        int piece = board.at(fromRow, fromCol);
        return std::abs(piece) == 5 && fromRow == toRow && std::abs(toCol - fromCol) == 2;
    }

    bool canCastle(int fromRow, int fromCol, int toRow, int toCol) const
    {
        int piece = board.at(fromRow, fromCol);
        int color = piece > 0 ? WHITE : BLACK;
        int homeRow = (color == WHITE) ? 7 : 0;
        bool kingSide = toCol > fromCol;

        if (fromRow != homeRow || fromCol != 4 || toRow != homeRow)
            return false;
        if (!castle.hasRight(color, kingSide))
            return false;

        int rookCol = kingSide ? 7 : 0;
        if (board.at(homeRow, rookCol) != 4 * color)
            return false;

        // all squares between king and rook must be empty
        int step = kingSide ? 1 : -1;
        for (int c = fromCol + step; c != rookCol; c += step)
            if (board.at(homeRow, c) != 0)
                return false;

        // the king can't castle out of, through, or into check
        for (int c = fromCol; c != toCol + step; c += step)
            if (isSquareAttacked(homeRow, c, -color))
                return false;

        return true;
    }

    bool isPseudoLegal(int fromRow, int fromCol, int toRow, int toCol) const
    {
        if (fromRow < 0 || fromRow > 7 || fromCol < 0 || fromCol > 7 ||
            toRow < 0 || toRow > 7 || toCol < 0 || toCol > 7)
            return false;
        int piece = board.at(fromRow, fromCol);
        if (piece == 0 || (piece > 0) != (turn > 0))
            return false;
        if (fromRow == toRow && fromCol == toCol)
            return false;
        if (sameTeam(board.at(toRow, toCol), piece))
            return false;

        switch (std::abs(piece))
        {
        case 1: return pawn.check_pawn(toRow, toCol, fromRow, fromCol, piece, board, epRow, epCol) != 0;
        case 2: return horse.check_horse(toRow, toCol, fromRow, fromCol, piece, board) != 0;
        case 3: return bis.check_bis(toRow, toCol, fromRow, fromCol, piece, board) != 0;
        case 4: return rook.check_rook(toRow, toCol, fromRow, fromCol, piece, board) != 0;
        case 6: return queen.check_queen(toRow, toCol, fromRow, fromCol, piece, board) != 0;
        case 5:
            if (isCastleMove(fromRow, fromCol, toRow, toCol))
                return canCastle(fromRow, fromCol, toRow, toCol);
            return king.check_king(toRow, toCol, fromRow, fromCol, piece, board) != 0;
        }
        return false;
    }

    // moves the pieces with no legality checks (callers check first)
    MoveResult applyMove(int fromRow, int fromCol, int toRow, int toCol, char promo)
    {
        MoveResult res;
        res.ok = true;
        int piece = board.at(fromRow, fromCol);
        int color = piece > 0 ? WHITE : BLACK;
        res.capture = board.at(toRow, toCol) != 0;

        // en passant: pawn goes diagonally onto the empty ep square, the passed pawn is removed
        if (std::abs(piece) == 1 && fromCol != toCol && board.at(toRow, toCol) == 0 &&
            toRow == epRow && toCol == epCol)
        {
            board.set(fromRow, toCol, 0);
            res.enPassant = true;
            res.capture = true;
        }

        // castling: also move the rook
        if (isCastleMove(fromRow, fromCol, toRow, toCol))
        {
            bool kingSide = toCol > fromCol;
            int rookFrom = kingSide ? 7 : 0;
            int rookTo = kingSide ? 5 : 3;
            board.set(fromRow, rookTo, board.at(fromRow, rookFrom));
            board.set(fromRow, rookFrom, 0);
            res.castle = true;
        }

        board.set(toRow, toCol, piece);
        board.set(fromRow, fromCol, 0);

        // promotion
        if (std::abs(piece) == 1 && (toRow == 0 || toRow == 7))
        {
            int newType = 6;
            if (promo == 'r') newType = 4;
            else if (promo == 'b') newType = 3;
            else if (promo == 'n') newType = 2;
            board.set(toRow, toCol, newType * color);
            res.promotion = true;
        }

        // new en passant square only right after a double pawn step
        epRow = epCol = -1;
        if (std::abs(piece) == 1 && std::abs(toRow - fromRow) == 2)
        {
            epRow = (fromRow + toRow) / 2;
            epCol = fromCol;
        }

        castle.update(fromRow, fromCol, toRow, toCol, piece);
        turn = -turn;
        return res;
    }
};
