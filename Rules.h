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
        /*θα κανουμε το βημα προς τα μπροστα. πρεπει να ελεγχο αν υπαρχει καποιος μπροστα του.δηλαδη στο θα εχω την θεση του rook και θα αφαιρεσω απο το 7. αν θεση+
        το αποτελεσμα τις αφαιρεσης ειναι κενη παει παντου αν δεν ειναι δεν παει*/
        int colcheck = selectedCol - col;
        int sum = 0;
        int target = board.at(row, col);
        if (selectedRow != row)
        {
            int clear = (selectedRow > row) ? selectedRow - row : row - selectedRow;
            // pros ta panw;
            if (selectedRow > row)
            {

                for (int i = selectedRow; i >= row; i--)
                {
                    if (board.at(i, col) == 0)
                    {
                        sum += 1;
                    }
                }
                if ((sum == clear || (board.at(row, col) < 0 && sum == (clear - 1))) && colcheck == 0)
                {
                    return 1;
                }
            }
            // pros ta piso
            else if (selectedRow < row)
            {

                for (int i = selectedRow; i <= row; i++)
                {
                    if (board.at(i, col) == 0)
                    {
                        sum += 1;
                    }
                }
                if ((sum == clear || (board.at(row, col) > 0 && sum == (clear - 1))) && colcheck == 0)
                    return 2;
            }
            else
                return 0;
        }
        // εδω παει προς αριστερα η δεξια :D
        else
        {
            int clearrow = abs(row - selectedRow);
            int clearcol = (selectedCol < col) ? col - selectedCol : selectedCol - col;

            if (selectedCol > col && clearrow == 0)
            {
                for (int i = selectedCol; i >= col; i--)
                    if (board.at(row, i) == 0)
                        sum += 1;

                if (sum == clearcol || (board.at(row, col) < 0 && sum == (clearcol - 1)))
                    return 2;
            }
            else if (selectedCol < col && clearrow == 0)
            {
                for (int i = selectedCol; i <= col; i++)
                    if (board.at(row, i == 0))
                        sum += 1;

                if (sum == clearcol || (board.at(row, col) > 0 && sum == (clearcol - 1)))
                    return 2;
            }
            else
                return 0;
        }
        return 0;
    }
};