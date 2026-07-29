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
            return 0; // (en passant not handled yet)
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
        int target = board.at(row, col); // destination square

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