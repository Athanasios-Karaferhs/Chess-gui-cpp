#pragma once
#include <cmath>
class pawnmove
{
public:
    int white_moved[8][8];
    int black_moved[8][8];

    pawnmove()
    {
        for (int i = 0; i < 8; i++)
        {
            for (int j = 0; j < 8; j++)
            {
                white_moved[i][j] = 0;
                black_moved[i][j] = 0;
            }
        }
    }
    int rowcheck = 0;
    int colCheck = -1;
    int answer = -1;

    int check_pawn(int row, int col, int selectedCol, int selectedRow, int value)
    {
        /*ετσω οτι στο row 1 που ειναι το μαυρο πιωνι θελω να παω 2 μπροστα(στο 3) θα πρεπει να κανω 3-1 βηματα*/
        rowcheck = abs(row - selectedRow);
        colCheck = col - selectedCol;

        if (rowcheck == 2 && colCheck == 0 && (white_moved[selectedRow][selectedCol] == 0 || black_moved[selectedRow][selectedCol] == 0))
        {
            if (value == 1)
            {
                if (selectedRow == 6)
                {
                    white_moved[selectedRow][selectedCol] = 1;
                    answer = 2;
                }
            }
            else
            {
                if (selectedRow == 1)
                {
                    black_moved[selectedRow][selectedCol] = 1;
                    answer = 2;
                }
            }
        }
        else if (rowcheck == 1 && colCheck == 0)
        {
            answer = 1;
        }
        else
        {
            answer = 0;
        }

        return answer;
    }
};