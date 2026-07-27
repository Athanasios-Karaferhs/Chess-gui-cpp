#pragma once

class Board
{
private:
    int squares[8][8];

public:
    Board()
    {
        setStartingPosition();
    }

    void setStartingPosition()
    {
        int startingBoard[8][8] = {
            {-4, -2, -3, -6, -5, -3, -2, -4},
            {-1, -1, -1, -1, -1, -1, -1, -1},
            {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0},
            {1, 1, 1, 1, 1, 1, 1, 1},
            {4, 2, 3, 6, 5, 3, 2, 4}};

        for (int row = 0; row < 8; row++)
            for (int col = 0; col < 8; col++)
                squares[row][col] = startingBoard[row][col];
    }

    int at(int row, int col) const
    {
        return squares[row][col];
    }

    void set(int trow, int tcol, int value)
    {
        squares[trow][tcol] = value;
    }
};