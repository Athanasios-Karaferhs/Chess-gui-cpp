#pragma once

class pawnmove
{
public:
    int rowcheck = 0;
    int colCheck = -1;
    int answer = -1;
    int check_black(int row, int col, int selectedCol, int selectedRow, int value)
    {
        rowcheck = row - selectedRow;
        colCheck = col - selectedCol;
        if (value == -1)
        {
            if (rowcheck == 2 && selectedRow == 1 && colCheck == 0)
            {
                answer = 2;
                rowcheck = 0;
            }
            else if (rowcheck == 1 && colCheck == 0)
            {
                answer = 1;
                rowcheck = 0;
            }
            else
            {
                rowcheck = 0;
                answer = 0;
            }
        }
        return answer;
    }
};