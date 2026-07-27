#include <SFML/Graphics.hpp>
#include <iostream>
#include "Board.h"

const int BOARD_SIZE = 8;
const int SQUARE_SIZE = 70;
const int BOARD_PIXELS = BOARD_SIZE * SQUARE_SIZE; // width/height of just the board
const int PANEL_WIDTH = 300;
const int WINDOW_WIDTH = BOARD_PIXELS + PANEL_WIDTH; // 840
const int WINDOW_HEIGHT = BOARD_PIXELS;              // 8
using namespace sf;

int main()
{
    RenderWindow window(
        VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT),
        "Chess :)");
    RectangleShape squares[BOARD_SIZE][BOARD_SIZE];
    RectangleShape sidePanel;
    RectangleShape separator;
    separator.setSize(Vector2f(4, WINDOW_HEIGHT)); // 4px wide, full height
    separator.setPosition(BOARD_PIXELS, 0);        // right at the board's edge
    separator.setFillColor(Color::Black);

    sidePanel.setSize(Vector2f(PANEL_WIDTH, WINDOW_HEIGHT));
    sidePanel.setPosition(BOARD_PIXELS, 0);
    sidePanel.setFillColor(Color(192, 192, 192));
    for (int row = 0; row < BOARD_SIZE; ++row)
    {
        for (int col = 0; col < BOARD_SIZE; ++col)
        {
            squares[row][col].setSize(Vector2f(SQUARE_SIZE, SQUARE_SIZE));
            squares[row][col].setPosition(col * SQUARE_SIZE, row * SQUARE_SIZE);
            bool isLight = (row + col) % 2 == 0;
            squares[row][col].setFillColor(
                isLight ? Color::White : Color(0, 153, 76));
        }
    }
    // load το font για τα γραμματα
    Font font;
    if (!font.loadFromFile("AdwaitaMono-Regular.ttf"))
    {
        std::cerr << "Failed to load font\n";
        return -1;
    }
    // φταχνω τα γραματα για τις γραμμες
    Text columnLabels[BOARD_SIZE];
    std::string letters = "abcdefgh";
    for (int i = 0; i < BOARD_SIZE; ++i)
    {
        columnLabels[i].setFont(font);
        columnLabels[i].setString(std::string(1, letters[i]));
        columnLabels[i].setCharacterSize(16);
        columnLabels[i].setFillColor(Color::Black);
        columnLabels[i].setPosition(i * SQUARE_SIZE + 5, WINDOW_HEIGHT - 20);
    }

    // εδω ειναι οι στιλες
    Text rowsLables[BOARD_SIZE];
    std::string numbers = "87654321";
    for (int i = 0; i < BOARD_SIZE; ++i)
    {
        rowsLables[i].setFont(font);
        rowsLables[i].setString(std::string(1, numbers[i]));
        rowsLables[i].setCharacterSize(16);
        rowsLables[i].setFillColor(Color::Black);
        rowsLables[i].setPosition(0, i * SQUARE_SIZE + 30);
    }

    // #######################################################################################################################################
    /*απο εδω και περα κανουμε load ολα τα μαυρα textures. μεχρο το επομενο παρομιο σχολιο ειναι ΜΟΝΟ μαυρα(note:ολα τα pieces ειναι το square που πρεπει
    να ειναι -1 λογο του μεγεθους του board)*/

    // load τον μαυρο πυργο
    Texture textureRook;
    if (!textureRook.loadFromFile("pieces/black_ro.png"))
    {
        std::cerr << "Failed to load texture rook";
        return -1;
    }

    // load black bisa
    Texture texturebis;
    if (!texturebis.loadFromFile("pieces/black_bis.png"))
    {
        std::cerr << "Failed to load texture bis ";
        return -1;
    }

    // load black horse
    Texture textureHorse;
    if (!textureHorse.loadFromFile("pieces/black_horse.png"))
    {
        std::cerr << "failed to load texture  horse";
        return -1;
    }

    // loab black queen
    Texture textureBqueen;
    if (!textureBqueen.loadFromFile("pieces/black_que.png"))
    {
        std::cerr << "failled to load texture queen";
        return -1;
    }

    Texture texturebking;
    if (!texturebking.loadFromFile("pieces/black_king.png"))
    {
        std::cerr << "failled to load texture king";
        return -1;
    }

    // black pawn :)
    Texture texturepawn;
    if (!texturepawn.loadFromFile("pieces/black_po.png"))
    {
        std::cerr << "failled to load texture pawn";
        return -1;
    }

    // #######################################################################################################################################
    // WHITE TEXTURE STARTS HERE!!!
    // #######################################################################################################################################
    Texture texturewro;
    if (!texturewro.loadFromFile("pieces/white_rook.png"))
    {
        std::cerr << "failed to load texture rook";
        return -1;
    }

    Texture textwbis;
    if (!textwbis.loadFromFile("pieces/white_bis.png"))
    {
        std::cerr << "failed to load texture bis";
        return -1;
    }

    Texture textwhorse;
    if (!textwhorse.loadFromFile("pieces/white_horse.png"))
    {
        std::cerr << "failed to load texture horse";
        return -1;
    }

    Texture textwqueen;
    if (!textwqueen.loadFromFile("pieces/white_queen.png"))
    {
        std::cerr << "Failed to laod white queen";
        return -1;
    }

    Texture textwking;
    if (!textwking.loadFromFile("pieces/white_king.png"))
    {
        std::cerr << "failed to load white king";
        return -1;
    }

    Texture texturewpawn;
    if (!texturewpawn.loadFromFile("pieces/white_po.png"))
    {
        std::cerr << "Failed to load white pawn";
        return -1;
    }

    // #######################################################################################################################################

    Board board;
    int value = 0;
    int selectedRow = -1, selectedCol = -1;
    // τι δουλευει μετα το ανοιγμα του παραθυρου
    while (window.isOpen())
    {
        Event event;
        while (window.pollEvent(event))
        {
            if (event.type == Event::Closed)
                window.close();

            if (event.type == Event::MouseButtonPressed)
            {
                int mouseX = event.mouseButton.x;
                int mouseY = event.mouseButton.y;
                int col = mouseX / SQUARE_SIZE;
                int row = mouseY / SQUARE_SIZE;

                if (selectedRow == -1)
                {
                    if (board.at(row, col) != 0)
                    {
                        selectedRow = row;
                        selectedCol = col;
                        value = board.at(row, col);
                    }
                }
                else
                {
                    board.set(row, col, value);
                    board.set(selectedRow, selectedCol, 0);
                    selectedRow = -1;
                    selectedCol = -1;
                }
            }
        }

        window.clear();

        window.draw(sidePanel);
        window.draw(separator);

        for (int row = 0; row < BOARD_SIZE; ++row)
            for (int col = 0; col < BOARD_SIZE; ++col)
                window.draw(squares[row][col]);

        for (int row = 0; row < 8; row++)
        {
            for (int col = 0; col < 8; col++)
            {
                int value = board.at(row, col);
                if (value == 0)
                    continue;
                Texture *correctTexture = nullptr;

                switch (value)
                {
                case 1:
                    correctTexture = &texturewpawn;
                    break;
                case 2:
                    correctTexture = &textwhorse;
                    break;
                case 3:
                    correctTexture = &textwbis;
                    break;
                case 4:
                    correctTexture = &texturewro;
                    break;
                case 5:
                    correctTexture = &textwking;
                    break;
                case 6:
                    correctTexture = &textwqueen;
                    break;
                case -1:
                    correctTexture = &texturepawn;
                    break;
                case -2:
                    correctTexture = &textureHorse;
                    break;
                case -3:
                    correctTexture = &texturebis;
                    break;
                case -4:
                    correctTexture = &textureRook;
                    break;
                case -5:
                    correctTexture = &texturebking;
                    break;
                case -6:
                    correctTexture = &textureBqueen;
                    break;
                }

                Sprite sprite(*correctTexture);
                sprite.setPosition(col * SQUARE_SIZE, row * SQUARE_SIZE);
                window.draw(sprite);
            }
        }

        // εκτυπωση γραμμων(απο το α μεχρι το h)
        for (int i = 0; i < BOARD_SIZE; ++i)
            window.draw(columnLabels[i]);
        // εκτυπωση στυλων
        for (int i = 0; i < BOARD_SIZE; ++i)
            window.draw(rowsLables[i]);

        window.display();
    }

    return 0;
}