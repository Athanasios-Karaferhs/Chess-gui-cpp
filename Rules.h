#pragma once
#include <cmath>
#include "Board.h"

class pawnmove
{
public:
    bool white_moved[8] = {false};
    bool black_moved[8] = {false};

    // 0 = illegal,1 = normal2 = double-step,3 = capture
    int check_pawn(int row, int col, int selectedCol, int selectedRow, int value, Board &board)
    {
        int rowDiff = row - selectedRow;
        int colDiff = col - selectedCol;
        int direction = (value > 0) ? -1 : 1; // white moves toward row 0, black toward row 7
        int targetValue = board.at(row, col);

        if (targetValue != 0 && ((targetValue > 0) == (value > 0)))
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
                bool alreadyMoved = (value > 0) ? white_moved[selectedCol] : black_moved[selectedCol];
                bool pathClear = board.at(selectedRow + direction, selectedCol) == 0;

                if (startingRow && !alreadyMoved && pathClear)
                {
                    (value > 0 ? white_moved[selectedCol] : black_moved[selectedCol]) = true;
                    return 2;
                }
            }
            return 0;
        }

        if (std::abs(colDiff) == 1 && rowDiff == direction) // διαγωνιος
        {
            if (targetValue != 0)
                return 3;
            return 0; // εν εχει εν πασσαντ και ΔΕΝ θα το φτιαξω συντομα
        }

        return 0;
    }
};

class rookmove
{
public:
    int check_rook(int row, int col, int selectedRow, int selectedCol, int value, Board &board)
    {
        int mover = board.at(selectedRow, selectedCol);
        int target = board.at(row, col);

        // ελεγχοι
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
            // εμποδιο ιδιας ομαδας
            if (target != 0 && ((target > 0) == (mover > 0))) // το target λαο τπ mover ελεγχουν αν ειναι ιδιας ομαδας
                return 0;

            return (row > selectedRow) ? 1 : 2;
        }
        else // horizontal move
        {
            int step = (col > selectedCol) ? 1 : -1;
            for (int i = selectedCol + step; i != col; i += step)
                if (board.at(row, i) != 0)
                    return 0;

            if (target != 0 && ((target > 0) == (mover > 0)))
                return 0;

            return (col > selectedCol) ? 1 : 2;
        }
    }
};

class horsemove
{
public:
    int check_horse(int row, int col, int selectedRow, int selectedCol, int value, Board &board)
    {
        int dRow = (row > selectedRow) ? row - selectedRow : selectedRow - row;
        int dCol = (col > selectedCol) ? col - selectedCol : selectedCol - col;

        bool isLShape = (dRow == 2 && dCol == 1) || (dRow == 1 && dCol == 2);
        if (!isLShape)
            return 0;

        int mover = board.at(selectedRow, selectedCol);
        int target = board.at(row, col);

        if (target != 0 && ((target > 0) == (mover > 0)))
            return 0;

        return 1;
    }
};

class kingmove
{
public:
    int check_king(int row, int col, int selectedRow, int selectedCol, int value, Board &board)
    {
        rookmove rook;
        int dRow = 0;
        int dCol = 0;
        // εστω οτι το cast ειναι αν εχει κουνιθει το καστρο
        if (row > selectedRow)
            dRow = row - selectedRow;
        else if (row < selectedRow)
            dRow = selectedRow - row;
        else
            dRow = 0;
        // o βασικιας ο ασπρος ειναι στο (7,4)
        if (col > selectedCol)
            dCol = col - selectedCol;
        else if (col < selectedCol)
            dCol = selectedCol - col;
        else
            dCol = 0;

        int mover = board.at(selectedRow, selectedCol);
        int target = board.at(row, col);

        if (target != 0 && ((target > 0) == (mover > 0)))
            return 0;
        // ΜΟΝΟ ΑΠΟ ΤΗΝ ΔΕΞΙΑ ΠΛΕΥΡΑ
        if ((dCol == 1 && dRow == 0) || (dCol == 0 && dRow == 1) || (dCol == 1 && dRow == 1) || (board.at(7, 4) == 5))
            return 1;

        return 0;
    }
};

class bisopmove
{
public:
    int check_bis(int row, int col, int selectedRow, int selectedCol, int value, Board &board)
    {
        if (std::abs(row - selectedRow) != std::abs(col - selectedCol))
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

        int mover = board.at(selectedRow, selectedCol);
        int target = board.at(row, col);

        if (target != 0 && ((target > 0) == (mover > 0)))
            return 0;

        return 1;
    }
};

class queenmove
{
public:
    int check_queen(int row, int col, int selectedRow, int selectedCol, int value, Board &board)
    {
        rookmove rook;
        bisopmove bis;

        bool forw = ((col - selectedCol == 0 && row - selectedRow != 0) || (col - selectedCol != 0 && row - selectedRow == 0));

        if (forw)
        {
            return rook.check_rook(row, col, selectedRow, selectedCol, value, board);
        }
        else if (std::abs(row - selectedRow) == std::abs(col - selectedCol))
        {
            return bis.check_bis(row, col, selectedRow, selectedCol, value, board);
        }

        return 0; // αν δεν μπαει ουτε μπροστα ουτε πλαγια
    }
};

class castling
{
private:
    bool flag = false;

public:
    rookmove rook_change;
    kingmove king_change;
    int check_for_castle(int row, int col, int selectedRow, int selectedCol, int value, Board &board)
    {
        int dRow = std::abs(row - selectedRow);
        int dCol = std::abs(col - selectedCol);
        if (value == 5 && dRow == 0 && dCol == 2 && board.at(7, 4) == 5 && board.at(7, 7) == 4)
        {
            if (flag == false)
            {
                if (rook_change.check_rook(7, 5, 7, 7, 4, board) != 0)
                {
                    if (king_change.check_king(7, 6, 7, 4, 5, board) != 0)
                    {
                        flag = true;
                        return 1;
                    }
                }
            }
        }
        return 0;
    }
};