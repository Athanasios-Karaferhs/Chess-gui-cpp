#include <SFML/Graphics.hpp>
#include <iostream>

const int BOARD_SIZE = 8;
const int SQUARE_SIZE = 70;
const int WINDOW_SIZE = BOARD_SIZE * SQUARE_SIZE;
using namespace sf;

int main()
{
    RenderWindow window(
        VideoMode(WINDOW_SIZE, WINDOW_SIZE),
        "Chess :)");

    RectangleShape squares[BOARD_SIZE][BOARD_SIZE];
    for (int row = 0; row < BOARD_SIZE; ++row)
    {
        for (int col = 0; col < BOARD_SIZE; ++col)
        {
            squares[row][col].setSize(Vector2f(SQUARE_SIZE, SQUARE_SIZE));
            squares[row][col].setPosition(col * SQUARE_SIZE, row * SQUARE_SIZE);
            bool isLight = (row + col) % 2 == 0;
            squares[row][col].setFillColor(
                isLight ? Color::White : Color::Blue);
        }
    }
    /*απο εδω και περα κανουμε load ολα τα μαυρα textures. μεχρο το επομενο παρομιο σχολιο ειναι ΜΟΝΟ μαυρα(note:ολα τα pieces ειναι το square που πρεπει
    να ειναι -1 λογο του μεγεθους του board)*/

    // load τον μαυρο πυργο
    Texture textureRook;
    if (!textureRook.loadFromFile("pieces/black_ro.png"))
    {
        std::cerr << "Failed to load texture rook";
        return -1;
    }
    Sprite blackRookSprite(textureRook); // το sprite κανει refrence στην φωτο και λεει που να βαλει
    blackRookSprite.setPosition(0 * SQUARE_SIZE, 0 * SQUARE_SIZE);

    // load black bisa
    Texture texturebis;
    if (!texturebis.loadFromFile("pieces/black_bis.png"))
    {
        std::cerr << "Failed to load texture bis ";
        return -1;
    }
    Sprite blackBishopSprite(texturebis);
    blackBishopSprite.setPosition(1 * SQUARE_SIZE, 0 * SQUARE_SIZE);

    // load black horse
    Texture textureHorse;
    if (!textureHorse.loadFromFile("pieces/black_horse.png"))
    {
        std::cerr << "failed to load texture  horse";
        return -1;
    }
    Sprite BlackHorseSprite(textureHorse);
    BlackHorseSprite.setPosition(2 * SQUARE_SIZE, 0 * SQUARE_SIZE);

    // loab black queen
    Texture textureBqueen;
    if (!textureBqueen.loadFromFile("pieces/black_que.png"))
    {
        std::cerr << "failled to load texture queen";
        return -1;
    }
    Sprite BlackQueenSprite(textureBqueen);
    BlackQueenSprite.setPosition(3 * SQUARE_SIZE, 0 * SQUARE_SIZE);

    Texture texturebking;
    if (!texturebking.loadFromFile("pieces/black_king.png"))
    {
        std::cerr << "failled to load texture king";
        return -1;
    }
    Sprite BlackKingSprite(texturebking);
    BlackKingSprite.setPosition(4 * SQUARE_SIZE, 0 * SQUARE_SIZE);

    // φιαχνω τις δευτερες και βαριεμαι πολυ γτ ξερω οτι ερχονται και τα πιωνια
    Sprite secondBbis(texturebis);
    secondBbis.setPosition(5 * SQUARE_SIZE, 0 * SQUARE_SIZE);
    Sprite secondBhorse(textureHorse);
    secondBhorse.setPosition(6 * SQUARE_SIZE, 0 * SQUARE_SIZE);
    Sprite secondBrook(textureRook);
    secondBrook.setPosition(7 * SQUARE_SIZE, 0 * SQUARE_SIZE);

    // black pawn :)
    Texture texturepawn;
    if (!texturepawn.loadFromFile("pieces/black_po.png"))
    {
        std::cerr << "failled to load texture pawn";
        return -1;
    }

    Sprite blackpawn[BOARD_SIZE];
    for (int i = 0; i < BOARD_SIZE; ++i)
    {
        blackpawn[i].setTexture(texturepawn);
        blackpawn[i].setPosition(i * SQUARE_SIZE, 1 * SQUARE_SIZE);
    }
    while (window.isOpen())
    {
        Event event;
        while (window.pollEvent(event))
        {
            if (event.type == Event::Closed)
                window.close();
        }

        window.clear();

        for (int row = 0; row < BOARD_SIZE; ++row)
            for (int col = 0; col < BOARD_SIZE; ++col)
                window.draw(squares[row][col]);
        // black pieces
        window.draw(blackBishopSprite);
        window.draw(BlackHorseSprite);
        window.draw(blackRookSprite);
        window.draw(BlackQueenSprite);
        window.draw(BlackKingSprite);
        window.draw(secondBbis);
        window.draw(secondBhorse);
        window.draw(secondBrook);

        for (int i = 0; i < BOARD_SIZE; ++i)
            window.draw(blackpawn[i]);
        /*black is done here!!!
        white is starting here*/

        window.display();
    }

    return 0;
}