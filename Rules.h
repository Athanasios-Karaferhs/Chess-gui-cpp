#pragma once
#include <cmath>
#include <cstdlib>
#include "Board.h"

// Each class answers only "can this piece GEOMETRICALLY go from (selectedRow,selectedCol) to (row,col)?"
// They do NOT check if the move leaves the own king in check - GameState does that.
// They have NO side effects (they used to change flags while checking, which broke things).
// Piece values: 1 pawn, 2 horse, 3 bishop, 4 rook, 5 king, 6 queen. Positive = white, negative = black.

inline bool sameTeam(int a, int b)
{
    return a != 0 && b != 0 && ((a > 0) == (b > 0));
}

class pawnmove
{
public:
    // 0 = illegal, 1 = normal, 2 = double-step, 3 = capture, 4 = en passant
    // (epRow, epCol) is the square a pawn may capture onto en passant (-1 if none)
    int check_pawn(int row, int col, int selectedRow, int selectedCol, int value, const Board &board,
                   int epRow = -1, int epCol = -1) const
    {
        int rowDiff = row - selectedRow;
        int colDiff = col - selectedCol;
        int direction = (value > 0) ? -1 : 1; // white moves toward row 0, black toward row 7
        int targetValue = board.at(row, col);

        if (sameTeam(targetValue, value))
            return 0;

        if (colDiff == 0)
        {
            if (targetValue != 0)
                return 0;

            if (rowDiff == direction)
                return 1;

            if (rowDiff == 2 * direction)
            {
                bool startingRow = (value > 0) ? (selectedRow == 6) : (selectedRow == 1);
                bool pathClear = board.at(selectedRow + direction, selectedCol) == 0;
                if (startingRow && pathClear)
                    return 2;
            }
            return 0;
        }

        if (std::abs(colDiff) == 1 && rowDiff == direction) // διαγωνιος
        {
            if (targetValue != 0)
                return 3;
            if (row == epRow && col == epCol)
                return 4; // εν πασσαντ :)
        }

        return 0;
    }

    // does a pawn of colour `value` standing at (selectedRow,selectedCol) attack (row,col)?
    bool attacks(int row, int col, int selectedRow, int selectedCol, int value) const
    {
        int direction = (value > 0) ? -1 : 1;
        return row - selectedRow == direction && std::abs(col - selectedCol) == 1;
    }
};

class rookmove
{
public:
    int check_rook(int row, int col, int selectedRow, int selectedCol, int value, const Board &board) const
    {
        int mover = board.at(selectedRow, selectedCol);
        int target = board.at(row, col);

        if (row != selectedRow && col != selectedCol)
            return 0;
        if (row == selectedRow && col == selectedCol)
            return 0;

        if (col == selectedCol) // vertical move
        {
            int step = (row > selectedRow) ? 1 : -1;
            for (int i = selectedRow + step; i != row; i += step)
                if (board.at(i, col) != 0)
                    return 0;
        }
        else // horizontal move
        {
            int step = (col > selectedCol) ? 1 : -1;
            for (int i = selectedCol + step; i != col; i += step)
                if (board.at(row, i) != 0)
                    return 0;
        }

        // εμποδιο ιδιας ομαδας
        if (sameTeam(target, mover))
            return 0;
        return 1;
    }
};

class horsemove
{
public:
    int check_horse(int row, int col, int selectedRow, int selectedCol, int value, const Board &board) const
    {
        int dRow = std::abs(row - selectedRow);
        int dCol = std::abs(col - selectedCol);

        bool isLShape = (dRow == 2 && dCol == 1) || (dRow == 1 && dCol == 2);
        if (!isLShape)
            return 0;

        if (sameTeam(board.at(row, col), board.at(selectedRow, selectedCol)))
            return 0;
        return 1;
    }
};

class kingmove
{
public:
    // only the normal one-square king move. Castling is handled by the castling class.
    int check_king(int row, int col, int selectedRow, int selectedCol, int value, const Board &board) const
    {
        int dRow = std::abs(row - selectedRow);
        int dCol = std::abs(col - selectedCol);

        if (dRow > 1 || dCol > 1 || (dRow == 0 && dCol == 0))
            return 0;
        if (sameTeam(board.at(row, col), board.at(selectedRow, selectedCol)))
            return 0;
        return 1;
    }
};

class bisopmove
{
public:
    int check_bis(int row, int col, int selectedRow, int selectedCol, int value, const Board &board) const
    {
        if (row == selectedRow || std::abs(row - selectedRow) != std::abs(col - selectedCol))
            return 0;

        int Rstep = (row > selectedRow) ? 1 : -1;
        int Cstep = (col > selectedCol) ? 1 : -1;

        int i = selectedRow + Rstep;
        int j = selectedCol + Cstep;
        while (i != row)
        {
            if (board.at(i, j) != 0)
                return 0;
            i += Rstep;
            j += Cstep;
        }

        if (sameTeam(board.at(row, col), board.at(selectedRow, selectedCol)))
            return 0;
        return 1;
    }
};

class queenmove
{
public:
    int check_queen(int row, int col, int selectedRow, int selectedCol, int value, const Board &board) const
    {
        rookmove rook;
        bisopmove bis;
        if (row == selectedRow || col == selectedCol)
            return rook.check_rook(row, col, selectedRow, selectedCol, value, board);
        return bis.check_bis(row, col, selectedRow, selectedCol, value, board);
    }
};

// Castling rights: they are lost forever once the king or that rook moves (or the rook is captured).
class castling
{
public:
    bool whiteKingSide = true, whiteQueenSide = true;
    bool blackKingSide = true, blackQueenSide = true;

    void reset()
    {
        whiteKingSide = whiteQueenSide = blackKingSide = blackQueenSide = true;
    }

    bool hasRight(int color, bool kingSide) const
    {
        if (color > 0)
            return kingSide ? whiteKingSide : whiteQueenSide;
        return kingSide ? blackKingSide : blackQueenSide;
    }

    // call after every move with the squares involved
    void update(int fromRow, int fromCol, int toRow, int toCol, int movedPiece)
    {
        if (movedPiece == 5)
            whiteKingSide = whiteQueenSide = false;
        if (movedPiece == -5)
            blackKingSide = blackQueenSide = false;

        // a rook leaving its corner, or something landing on the corner (capturing the rook)
        int squares[2][2] = {{fromRow, fromCol}, {toRow, toCol}};
        for (auto &sq : squares)
        {
            if (sq[0] == 7 && sq[1] == 7) whiteKingSide = false;
            if (sq[0] == 7 && sq[1] == 0) whiteQueenSide = false;
            if (sq[0] == 0 && sq[1] == 7) blackKingSide = false;
            if (sq[0] == 0 && sq[1] == 0) blackQueenSide = false;
        }
    }
};
