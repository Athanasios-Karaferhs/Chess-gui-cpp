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
    kingmove king;
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
                if (type == 5 && king.check_king(Krow, Kcol, i, j, piece, board) != 0)
                    return true;
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
class checkmate
{
public:
    bool isCheckmate(int turn, Board &board)
    {
        kingmove king;
        can_it_move check;

        if (!check.checker(turn, board))
            return false;

        int kingValue = (turn == WHITE) ? 5 : -5;

        int Krow = -1, Kcol = -1;
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

        for (int i = Krow - 1; i <= Krow + 1; i++)
        {
            for (int j = Kcol - 1; j <= Kcol + 1; j++)
            {

                if (king.check_king(i, j, Krow, Kcol, kingValue, board) == 0)
                    continue;

                int captured = board.at(i, j);
                board.set(i, j, kingValue);
                board.set(Krow, Kcol, 0);

                bool stillInCheck = check.checker(turn, board);
                board.set(Krow, Kcol, kingValue);
                board.set(i, j, captured);

                if (!stillInCheck)
                    return false;
            }
        }

        return true;
    }
};