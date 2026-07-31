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