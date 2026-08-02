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

    bool checker(int value, Board &board)
    {
        int Krow = -1;
        int Kcol = -1;
        int kingValue = (value == WHITE) ? 5 : -5;

        // βρισκω που βρισκεται ο βασιλιας που ελεγχω
        for (int i = 0; i < 8; i++)
        {
            for (int j = 0; j < 8; j++)
            {
                if (board.at(i, j) == kingValue)
                {
                    Krow = i;
                    Kcol = j;
                }
            }
        }

        // τωρα ψαχνω ολο το board για εχθρικα κομματια που μπορουν να φτασουν εκει
        for (int i = 0; i < 8; i++)
        {
            for (int j = 0; j < 8; j++)
            {
                int piece = board.at(i, j);
                if (piece == 0)
                    continue;

                bool isEnemy = (value == WHITE) ? (piece < 0) : (piece > 0);
                if (!isEnemy)
                    continue;

                int type = std::abs(piece);

                if (type == 2 && horse.check_horse(Krow, Kcol, i, j, piece, board) != 0)
                    return true;
                if (type == 6 && queen.check_queen(Krow, Kcol, i, j, piece, board) != 0)
                    return true;
                if (type == 3 && bis.check_bis(Krow, Kcol, i, j, piece, board) != 0)
                    return true;
                if (type == 4 && rook.check_rook(Krow, Kcol, i, j, piece, board) != 0)
                    return true;
                if (type == 1 && pawn.check_pawn(Krow, Kcol, j, i, piece, board) == 3)
                    return true;
            }
        }

        return false;
    }
};