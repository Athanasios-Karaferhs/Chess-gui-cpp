#pragma once
#define WHITE 1
#define BLACK -1
class move
{
public:
    int change_turn(int turn)
    {
        if (turn == WHITE)
            turn = BLACK;
        else
            turn = WHITE;
        //-1 μαυρο,1 ασπρο
        return turn;
    }
};

class can_it_move
{
public:
    queenmove queen;
    rookmove rook;
    bisopmove bis;
    horsemove horse;
    pawnmove pawn;
    bool check = false;
    bool checker(int selectedRow, int selectedCol, int value, Board &board)
    {
        int Krow;
        int Kcol;
        // βρισκω που βρισκεται ο βασιλιας
        for (int i = 0; i < 8; i++)
        {
            for (int j = 0; j < 8; j++)
            {
                if (board.at(i, j) == -5)
                {
                    Krow = i;
                    Kcol = j;
                }
            }
        }

        // θα ελεγχω αν το καθε πιονι μπορει να παει στην παραπανω θεση.
        if (horse.check_horse(selectedRow, selectedCol, Krow, Kcol, value, board) != 0)
            check = true;
        else if (queen.check_queen(selectedRow, selectedCol, Krow, Kcol, value, board) != 0)
            check = true;
        else if (bis.check_bis(selectedRow, selectedCol, Krow, Kcol, value, board) != 0)
            check = true;
        else if (rook.check_rook(selectedRow, selectedCol, Krow, Kcol, value, board) != 0)
            check = true;
        // για το πιονει πρεπει να δω καποια πραγματα
        return check;
    }
};