#include <SFML/Graphics.hpp>
#include <iostream>

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
    // #######################################################################################################################################
    // WHITE TEXTURE STARTS HERE!!!
    // #######################################################################################################################################
    Texture texturewro;
    if (!texturewro.loadFromFile("pieces/white_rook.png"))
    {
        std::cerr << "failed to load texture rook";
        return -1;
    }
    Sprite whitero(texturewro);
    whitero.setPosition(7 * SQUARE_SIZE, 7 * SQUARE_SIZE);

    Texture textwbis;
    if (!textwbis.loadFromFile("pieces/white_bis.png"))
    {
        std::cerr << "failed to load texture bis";
        return -1;
    }
    Sprite whitebis(textwbis);
    whitebis.setPosition(5 * SQUARE_SIZE, 7 * SQUARE_SIZE);

    Texture textwhorse;
    if (!textwhorse.loadFromFile("pieces/white_horse.png"))
    {
        std::cerr << "failed to load texture horse";
        return -1;
    }
    Sprite whitehorse(textwhorse);
    whitehorse.setPosition(6 * SQUARE_SIZE, 7 * SQUARE_SIZE);

    Texture textwqueen;
    if (!textwqueen.loadFromFile("pieces/white_queen.png"))
    {
        std::cerr << "Failed to laod white queen";
        return -1;
    }
    Sprite whitequeen(textwqueen);
    whitequeen.setPosition(3 * SQUARE_SIZE, 7 * SQUARE_SIZE);

    Texture textwking;
    if (!textwking.loadFromFile("pieces/white_king.png"))
    {
        std::cerr << "failed to load white king";
        return -1;
    }
    Sprite whiteking(textwking);
    whiteking.setPosition(4 * SQUARE_SIZE, 7 * SQUARE_SIZE);

    Sprite secwbis(textwbis);
    secwbis.setPosition(2 * SQUARE_SIZE, 7 * SQUARE_SIZE);
    Sprite secwhor(textwhorse);
    secwhor.setPosition(1 * SQUARE_SIZE, 7 * SQUARE_SIZE);
    Sprite secwrok(texturewro);
    secwrok.setPosition(0 * SQUARE_SIZE, 7 * SQUARE_SIZE);

    Texture texturewpawn;
    if (!texturewpawn.loadFromFile("pieces/white_po.png"))
    {
        std::cerr << "Failed to load white pawn";
        return -1;
    }
    Sprite whitepawn[BOARD_SIZE];
    for (int i = 0; i < BOARD_SIZE; ++i)
    {
        whitepawn[i].setTexture(texturewpawn);
        whitepawn[i].setPosition(i * SQUARE_SIZE, 6 * SQUARE_SIZE);
    }

    // #######################################################################################################################################
    //  προσοχη τι βαζβ απο εδω και κατω
    while (window.isOpen())
    {
        Event event;
        while (window.pollEvent(event))
        {
            if (event.type == Event::Closed)
                window.close();
        }

        window.clear();

        window.draw(sidePanel);
        window.draw(separator);

        for (int row = 0; row < BOARD_SIZE; ++row)
            for (int col = 0; col < BOARD_SIZE; ++col)
                window.draw(squares[row][col]);

        // εκτυπωση γραμμων(απο το α μεχρι το h)
        for (int i = 0; i < BOARD_SIZE; ++i)
            window.draw(columnLabels[i]);
        // εκτυπωση στυλων
        for (int i = 0; i < BOARD_SIZE; ++i)
            window.draw(rowsLables[i]);

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
        // black is done here!!!

        // white is starting here
        window.draw(whitero);
        window.draw(whitehorse);
        window.draw(whitebis);
        window.draw(whiteking);
        window.draw(whitequeen);
        window.draw(secwbis);
        window.draw(secwhor);
        window.draw(secwrok);
        for (int i = 0; i < BOARD_SIZE; ++i)
            window.draw(whitepawn[i]);

        window.display();
    }

    return 0;
}