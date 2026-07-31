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
    /*  col ---->
        row
        |
        |
        \/
    */
    int Krow;
    int Kcol;
    // βρισκω που βρισκεται ο βασιλιας
    for (int i = 0; i < 64; i++)
    {
        for (int j = 0; j < 64; j++)
        {
            if (board.at(i, j) == 5 || board.at(i, j) == -5)
            {
                Krow = j;
                Kcol = i;
            }
        }
    }
};