#include "Pandora/Windowing/Window.h"
#include "Pandora/Graphics/Renderer.h"
#include "Pandora/Mathematics/Vector2.h"
#include "Pandora/Image.h"

#include <filesystem>
#include <optional>
#include <compare>
#include <array>
#include <map>

using namespace Pandora;

enum class ChessPieceType {
    None,
    OutOfBounds,
    QueenWhite,
    RookWhite,
    BishopWhite,
    KnightWhite,
    PawnWhite,
    KingWhite,
    QueenBlack,
    RookBlack,
    BishopBlack,
    KnightBlack,
    PawnBlack,
    KingBlack,
};

static bool isWhitePiece(ChessPieceType type) {
    return type == ChessPieceType::QueenWhite
        || type == ChessPieceType::RookWhite
        || type == ChessPieceType::BishopWhite
        || type == ChessPieceType::KnightWhite
        || type == ChessPieceType::PawnWhite
        || type == ChessPieceType::KingWhite;
}

static bool isBlackPiece(ChessPieceType type) {
    return type == ChessPieceType::QueenBlack
        || type == ChessPieceType::RookBlack
        || type == ChessPieceType::BishopBlack
        || type == ChessPieceType::KnightBlack
        || type == ChessPieceType::PawnBlack
        || type == ChessPieceType::KingBlack;
}

static ChessPieceType getChessPieceFromFileName(std::string_view name) {
    if (name.starts_with("BishopWhite")) {
        return ChessPieceType::BishopWhite;
    } else if (name.starts_with("RookWhite")) {
        return ChessPieceType::RookWhite;
    } else if (name.starts_with("QueenWhite")) {
        return ChessPieceType::QueenWhite;
    } else if (name.starts_with("KnightWhite")) {
        return ChessPieceType::KnightWhite;
    } else if (name.starts_with("PawnWhite")) {
        return ChessPieceType::PawnWhite;
    } else if (name.starts_with("KingWhite")) {
        return ChessPieceType::KingWhite;
    } else if (name.starts_with("BishopWhite")) {
        return ChessPieceType::BishopWhite;
    } else if (name.starts_with("RookBlack")) {
        return ChessPieceType::RookBlack;
    } else if (name.starts_with("QueenBlack")) {
        return ChessPieceType::QueenBlack;
    } else if (name.starts_with("KnightBlack")) {
        return ChessPieceType::KnightBlack;
    } else if (name.starts_with("PawnBlack")) {
        return ChessPieceType::PawnBlack;
    } else if (name.starts_with("KingBlack")) {
        return ChessPieceType::KingBlack;
    } else if (name.starts_with("BishopBlack")) {
        return ChessPieceType::BishopBlack;
    } else {
        throw std::runtime_error("Unknown chess piece image name");
    }
}

static std::map<ChessPieceType, u32> createChessPieceTextureMap(Renderer& renderer) {
    auto textureMap = std::map<ChessPieceType, u32>{};
    
    const auto chessPieceDirectoryPath = std::filesystem::path{ "./Assets/ChessPieces" };
    if (!std::filesystem::exists(chessPieceDirectoryPath)) {
        throw std::runtime_error("Asset folder does not exist");
    }

    for (const auto& entry : std::filesystem::directory_iterator{ chessPieceDirectoryPath }) {
        const auto fileName = entry.path().filename().string();

        const auto chessPiece = getChessPieceFromFileName(fileName);
        const auto chessPieceImage = Image::load(entry.path());
        const auto chessPieceTexture = renderer.createTexture(chessPieceImage);

        textureMap.emplace(chessPiece, chessPieceTexture);
    }

    return textureMap;
}

struct GridIndices {
    usize row{};
    usize column{};

    auto operator<=>(const GridIndices&) const = default;
};

struct ChessPiece {
    ChessPieceType type{};
    GridIndices position{};
};

struct GridSelection {
    std::vector<GridIndices> movementSquares{};
    ChessPiece piece{};
};

enum class DirectionType {
    Up,
    Down,
    Left,
    Right,
    DiagonalLeftUp,
    DiagonalLeftDown,
    DiagonalRightUp,
    DiagonalRightDown,
};

enum class MovementResultType {
    Move,
    MoveAndCaptured,
    Impossible,
};

enum class PlayerTurn {
    White,
    Black
};

static PlayerTurn getNextPlayerTurn(PlayerTurn turn) {
    if (turn == PlayerTurn::White) {
        return PlayerTurn::Black;
    } else {
        return PlayerTurn::White;
    }
}

static bool isPlayerTurnPiece(ChessPieceType pieceType, PlayerTurn turn) {
    return turn == PlayerTurn::White && isWhitePiece(pieceType)
        || turn == PlayerTurn::Black && isBlackPiece(pieceType);
}

struct ChessBoard {
    static constexpr usize BoardSquareSize = 8;
    static constexpr usize BoardSquarePixelSize = 120;

    using BoardArray = std::array<ChessPieceType, BoardSquareSize * BoardSquareSize>;

    GridIndices getGridIndicesFromArrayIndex(usize index) const {
        const auto row = index / BoardSquareSize;
        const auto column = index % BoardSquareSize;
        return{ row, column };
    }

    GridIndices getGridIndicesFromScreenPosition(usize x, usize y) {
        const auto row = std::clamp(y / BoardSquarePixelSize, 0ull, BoardSquareSize - 1);
        const auto column = std::clamp(x / BoardSquarePixelSize, 0ull, BoardSquareSize - 1);
        return{ row, column };
    }

    ChessPieceType getSquareFromGridIndices(GridIndices position) {
        const auto index = position.row * BoardSquareSize + position.column;
        return board[index];
    }

    bool isOutOfBoundsMovement(GridIndices position, DirectionType direction) {
        const auto maximumIndex = BoardSquareSize - 1;
        return direction == DirectionType::Up && position.row == 0
            || direction == DirectionType::Down && position.row == maximumIndex
            || direction == DirectionType::Left && position.column == 0
            || direction == DirectionType::Right && position.column == maximumIndex
            || direction == DirectionType::DiagonalLeftUp && (position.column == 0 || position.row == 0)
            || direction == DirectionType::DiagonalLeftDown && (position.column == 0 || position.row == maximumIndex)
            || direction == DirectionType::DiagonalRightUp && (position.column == maximumIndex || position.row == 0)
            || direction == DirectionType::DiagonalRightDown && (position.column == maximumIndex || position.row == maximumIndex);
    }

    std::optional<GridIndices> getGridIndicesFromMovement(GridIndices position, DirectionType movement) {
        if (isOutOfBoundsMovement(position, movement)) {
            return {};
        }

        if (movement == DirectionType::Up) {
            return GridIndices{ position.row - 1, position.column };
        } else if (movement == DirectionType::Down) {
            return GridIndices{ position.row + 1, position.column };
        } else if (movement == DirectionType::Left) {
            return GridIndices{ position.row, position.column - 1 };
        } else if (movement == DirectionType::Right) {
            return GridIndices{ position.row, position.column + 1 };
        } else if (movement == DirectionType::DiagonalLeftUp) {
            return GridIndices{ position.row - 1, position.column - 1 };
        } else if (movement == DirectionType::DiagonalLeftDown) {
            return GridIndices{ position.row + 1, position.column - 1 };
        } else if (movement == DirectionType::DiagonalRightUp) {
            return GridIndices{ position.row - 1, position.column + 1 };
        } else if (movement == DirectionType::DiagonalRightDown) {
            return GridIndices{ position.row + 1, position.column + 1 };
        }

        throw std::runtime_error("Unhandled square movement");
    }

    ChessPiece getPieceInDirection(GridIndices position, DirectionType movement) {
        const auto gridIndices = getGridIndicesFromMovement(position, movement);
        if (gridIndices.has_value()) {
            const auto piece = getSquareFromGridIndices(*gridIndices);
            return ChessPiece{ piece, *gridIndices };
        } else {
            return ChessPiece{ ChessPieceType::OutOfBounds, position };
        }
    }

    bool isTakeable(ChessPiece piece, ChessPiece targetPiece) {
        return isWhitePiece(piece.type) && isBlackPiece(targetPiece.type)
            || isBlackPiece(piece.type) && isWhitePiece(targetPiece.type);
    }

    MovementResultType addPieceMovementSquare(std::vector<GridIndices>& squares, ChessPiece piece, DirectionType movement, bool isCapturePossible = true, bool isOnlyCapture = false) {
        const auto nextPieceInDirection = getPieceInDirection(piece.position, movement);
        if (nextPieceInDirection.type != ChessPieceType::None && isCapturePossible && isTakeable(piece, nextPieceInDirection)) {
            squares.push_back(nextPieceInDirection.position);
            return MovementResultType::MoveAndCaptured;
        } else if (nextPieceInDirection.type == ChessPieceType::None && !isOnlyCapture) {
            squares.push_back(nextPieceInDirection.position);
            return MovementResultType::Move;
        } else {
            return MovementResultType::Impossible;
        }
    }

    void addPieceMovementSquares(std::vector<GridIndices>& squares, ChessPiece piece, DirectionType movement, bool isCapturePossible = true, usize maximum = 0) {
        const auto maximumMovementCount = maximum == 0 ? BoardSquareSize * BoardSquareSize : maximum;

        auto currentPosition = piece.position;
        auto currentMovementCount = 0;

        while (currentMovementCount != maximumMovementCount) {
            const auto movementPiece = ChessPiece{ piece.type, currentPosition };
            const auto movementResult = addPieceMovementSquare(squares, movementPiece, movement, isCapturePossible);

            if (movementResult == MovementResultType::MoveAndCaptured || movementResult == MovementResultType::Impossible) {
                break;
            }

            currentPosition = squares.back();
            currentMovementCount += 1;
        }
    }

    usize getPawnSquareMovementCount(GridIndices position) {
        if (position.row == 1 || position.row == 6) {
            return 2;
        } else {
            return 1;
        }
    }

    void addKnightMovementSquares(
        std::vector<GridIndices>& squares, 
        ChessPiece knightPiece, 
        DirectionType diagonalDirection, 
        DirectionType verticalDirection, 
        DirectionType horizontalDirection
    ) {
        const auto diagonalPiece = getPieceInDirection(knightPiece.position, diagonalDirection);
        if (diagonalPiece.type != ChessPieceType::OutOfBounds) {
            const auto knightMovementPiece = ChessPiece{ knightPiece.type, diagonalPiece.position };
            addPieceMovementSquare(squares, knightMovementPiece, verticalDirection);
            addPieceMovementSquare(squares, knightMovementPiece, horizontalDirection);
        }
    }

    ChessPiece findPieceInDirection(GridIndices position, DirectionType direction) {
        auto nextPiece = getPieceInDirection(position, direction);
        while (nextPiece.type == ChessPieceType::None && nextPiece.type != ChessPieceType::OutOfBounds) {
            nextPiece = getPieceInDirection(nextPiece.position, direction);
        }

        return nextPiece;
    }

    bool isPieceUnderThreatFromDirection(ChessPiece piece, DirectionType direction) {
        const auto pieceInDirection = findPieceInDirection(piece.position, direction);
        if (isTakeable(pieceInDirection, piece)) {
            const auto movementSquares = getPieceMovementSquaresUnrestricted(pieceInDirection);
            const auto isUnderThreat = std::find(movementSquares.begin(), movementSquares.end(), piece.position);
            return isUnderThreat != movementSquares.end();
        }

        return false;
    }

    bool isPieceUnderThreat(ChessPiece piece) {
        auto directions = {
            DirectionType::DiagonalLeftDown,
            DirectionType::DiagonalLeftUp,
            DirectionType::DiagonalRightDown,
            DirectionType::DiagonalRightUp,
            DirectionType::Up,
            DirectionType::Down,
            DirectionType::Left,
            DirectionType::Right
        };

        for (auto direction : directions) {
            if (isPieceUnderThreatFromDirection(piece, direction)) {
                return true;
            }
        }

        return false;
    }

    ChessPiece findPiece(ChessPieceType type) {
        const auto position = std::find(board.begin(), board.end(), type);
        const auto index = std::distance(board.begin(), position);
        const auto indices = getGridIndicesFromArrayIndex(index);

        return ChessPiece{ type, indices };
    }

    ChessPiece getKingPieceForPlayer(PlayerTurn turn) {
        if (turn == PlayerTurn::White) {
            return findPiece(ChessPieceType::KingWhite);
        } else {
            return findPiece(ChessPieceType::KingBlack);
        }
    }

    bool isKingUnderThreatAfterMove(ChessPiece piece, PlayerTurn turn) {
        const auto pieceIndex = piece.position.row * BoardSquareSize + piece.position.column;
        const auto playerKing = getKingPieceForPlayer(turn);

        if (playerKing.type == piece.type) {
            return false;
        }

        board[pieceIndex] = ChessPieceType::None;
        const auto isUnderThreat = isPieceUnderThreat(playerKing);
        board[pieceIndex] = piece.type;

        return isUnderThreat;
    }

    bool isPlayerUnderCheck(PlayerTurn turn) const {
        return turn == PlayerTurn::White && isWhiteUnderCheck
            || turn == PlayerTurn::Black && isBlackUnderCheck;
    }

    std::vector<GridIndices> getPieceMovementSquaresStoppingCheck(ChessPiece piece, PlayerTurn turn) {
        const auto playerKing = getKingPieceForPlayer(turn);
        const auto pieceMovementSquares = getPieceMovementSquaresUnrestricted(piece);

        if (playerKing.type == piece.type) {
            return pieceMovementSquares;
        }

        auto validMovementSquares = std::vector<GridIndices>{};
        for (const auto& movementSquare : pieceMovementSquares) {
            const auto movementSquareIndex = movementSquare.row * BoardSquareSize + movementSquare.column;
            const auto movementSquareOldType = board[movementSquareIndex];

            board[movementSquareIndex] = piece.type;
            const auto isUnderThreat = isPieceUnderThreat(playerKing);
            board[movementSquareIndex] = movementSquareOldType;

            if (!isUnderThreat) {
                validMovementSquares.push_back(movementSquare);
            }
        }

        return validMovementSquares;
    }

    std::vector<GridIndices> getPieceMovementSquares(ChessPiece piece, PlayerTurn turn) {
        if (isPlayerUnderCheck(turn)) {
            return getPieceMovementSquaresStoppingCheck(piece, turn);
        } else if (!isKingUnderThreatAfterMove(piece, turn)) {
            return getPieceMovementSquaresUnrestricted(piece);
        } else {
            return {};
        }
    }

    void addKingMovementSquare(std::vector<GridIndices>& squares, ChessPiece piece, DirectionType direction) {
        const auto movementPosition = getGridIndicesFromMovement(piece.position, direction);
        if (!movementPosition.has_value()) {
            return;
        }

        const auto movementSquareIndex = movementPosition->row * BoardSquareSize + movementPosition->column;
        const auto movementSquareOldType = board[movementSquareIndex];

        board[movementSquareIndex] = piece.type;
        const auto isUnderThreat = isPieceUnderThreat(ChessPiece{ piece.type, *movementPosition });
        board[movementSquareIndex] = movementSquareOldType;

        if (!isUnderThreat) {
            addPieceMovementSquare(squares, piece, direction);
        }
    }

    std::vector<GridIndices> getPieceMovementSquaresUnrestricted(ChessPiece piece) {
        auto result = std::vector<GridIndices>{};
        if (piece.type == ChessPieceType::PawnBlack) {
            const auto movementCount = getPawnSquareMovementCount(piece.position);
            addPieceMovementSquares(result, piece, DirectionType::Up, false, movementCount);
            addPieceMovementSquare(result, piece, DirectionType::DiagonalLeftUp, true, true);
            addPieceMovementSquare(result, piece, DirectionType::DiagonalRightUp, true, true);
        } else if (piece.type == ChessPieceType::PawnWhite) {
            const auto movementCount = getPawnSquareMovementCount(piece.position);
            addPieceMovementSquares(result, piece, DirectionType::Down, false, movementCount);
            addPieceMovementSquare(result, piece, DirectionType::DiagonalLeftDown, true, true);
            addPieceMovementSquare(result, piece, DirectionType::DiagonalRightDown, true, true);
        } else if (piece.type == ChessPieceType::RookBlack || piece.type == ChessPieceType::RookWhite) {
            addPieceMovementSquares(result, piece, DirectionType::Left);
            addPieceMovementSquares(result, piece, DirectionType::Right);
            addPieceMovementSquares(result, piece, DirectionType::Up);
            addPieceMovementSquares(result, piece, DirectionType::Down);
        } else if (piece.type == ChessPieceType::BishopBlack || piece.type == ChessPieceType::BishopWhite) {
            addPieceMovementSquares(result, piece, DirectionType::DiagonalLeftDown);
            addPieceMovementSquares(result, piece, DirectionType::DiagonalLeftUp);
            addPieceMovementSquares(result, piece, DirectionType::DiagonalRightDown);
            addPieceMovementSquares(result, piece, DirectionType::DiagonalRightUp);
        } else if (piece.type == ChessPieceType::QueenBlack || piece.type == ChessPieceType::QueenWhite) {
            addPieceMovementSquares(result, piece, DirectionType::Left);
            addPieceMovementSquares(result, piece, DirectionType::Right);
            addPieceMovementSquares(result, piece, DirectionType::Up);
            addPieceMovementSquares(result, piece, DirectionType::Down);
            addPieceMovementSquares(result, piece, DirectionType::DiagonalLeftDown);
            addPieceMovementSquares(result, piece, DirectionType::DiagonalLeftUp);
            addPieceMovementSquares(result, piece, DirectionType::DiagonalRightDown);
            addPieceMovementSquares(result, piece, DirectionType::DiagonalRightUp);
        } else if (piece.type == ChessPieceType::KingBlack || piece.type == ChessPieceType::KingWhite) {
            addKingMovementSquare(result, piece, DirectionType::Left);
            addKingMovementSquare(result, piece, DirectionType::Right);
            addKingMovementSquare(result, piece, DirectionType::Up);
            addKingMovementSquare(result, piece, DirectionType::Down);
            addKingMovementSquare(result, piece, DirectionType::DiagonalLeftDown);
            addKingMovementSquare(result, piece, DirectionType::DiagonalLeftUp);
            addKingMovementSquare(result, piece, DirectionType::DiagonalRightDown);
            addKingMovementSquare(result, piece, DirectionType::DiagonalRightUp);
        } else if (piece.type == ChessPieceType::KnightBlack || piece.type == ChessPieceType::KnightWhite) {
            addKnightMovementSquares(result, piece, DirectionType::DiagonalLeftUp, DirectionType::Up, DirectionType::Left);
            addKnightMovementSquares(result, piece, DirectionType::DiagonalRightUp, DirectionType::Up, DirectionType::Right);
            addKnightMovementSquares(result, piece, DirectionType::DiagonalLeftDown, DirectionType::Down, DirectionType::Left);
            addKnightMovementSquares(result, piece, DirectionType::DiagonalRightDown, DirectionType::Down, DirectionType::Right);
        }

        return result;
    }

    std::vector<ChessPiece> getPlayerPieces() const {
        auto result = std::vector<ChessPiece>{};
        for (auto index = 0; index < board.size(); index++) {
            const auto chessPieceType = board[index];
            if (isPlayerTurnPiece(chessPieceType, turn)) {
                const auto gridIndices = getGridIndicesFromArrayIndex(index);
                result.push_back(ChessPiece{ chessPieceType, gridIndices });
            }
        }

        return result;
    }
    
    bool isCheckMateOrStaleMate() {
        const auto playerPieces = getPlayerPieces();
        for (const auto& piece : playerPieces) {
            const auto pieceMovementSquares = getPieceMovementSquares(piece, turn);
            if (!pieceMovementSquares.empty()) {
                return false;
            }
        }

        return true;
    }

    void movePieceUnchecked(ChessPiece piece, GridIndices targetLocation) {
        isWhiteUnderCheck = false;
        isBlackUnderCheck = false;

        const auto pieceIndex = piece.position.row * BoardSquareSize + piece.position.column;
        const auto targetLocationIndex = targetLocation.row * BoardSquareSize + targetLocation.column;

        board[pieceIndex] = ChessPieceType::None;
        board[targetLocationIndex] = piece.type;

        const auto enemyKing = getKingPieceForPlayer(getNextPlayerTurn(turn));
        const auto isEnemyKingUnderThreat = isPieceUnderThreat(enemyKing);

        isWhiteUnderCheck = isEnemyKingUnderThreat && turn == PlayerTurn::Black;
        isBlackUnderCheck = isEnemyKingUnderThreat && turn == PlayerTurn::White;

        turn = getNextPlayerTurn(turn);

        const auto isGameOver = isCheckMateOrStaleMate();
        if (isEnemyKingUnderThreat && isGameOver) {
            isCheckMate = true;
        } else if (isGameOver) {
            isStaleMate = true;
        }
    }

    BoardArray board{};
    PlayerTurn turn{};

    bool isWhiteUnderCheck{};
    bool isBlackUnderCheck{};

    bool isCheckMate{};
    bool isStaleMate{};
};

static ChessBoard generateStartingPositions() {
    auto startingPositions = ChessBoard::BoardArray{};

    startingPositions[0] = ChessPieceType::RookWhite;
    startingPositions[1] = ChessPieceType::KnightWhite;
    startingPositions[2] = ChessPieceType::BishopWhite;
    startingPositions[3] = ChessPieceType::QueenWhite;
    startingPositions[4] = ChessPieceType::KingWhite;
    startingPositions[5] = ChessPieceType::BishopWhite;
    startingPositions[6] = ChessPieceType::KnightWhite;
    startingPositions[7] = ChessPieceType::RookWhite;

    startingPositions[8] = ChessPieceType::PawnWhite;
    startingPositions[9] = ChessPieceType::PawnWhite;
    startingPositions[10] = ChessPieceType::PawnWhite;
    startingPositions[11] = ChessPieceType::PawnWhite;
    startingPositions[12] = ChessPieceType::PawnWhite;
    startingPositions[13] = ChessPieceType::PawnWhite;
    startingPositions[14] = ChessPieceType::PawnWhite;
    startingPositions[15] = ChessPieceType::PawnWhite;

    startingPositions[48] = ChessPieceType::PawnBlack;
    startingPositions[49] = ChessPieceType::PawnBlack;
    startingPositions[50] = ChessPieceType::PawnBlack;
    startingPositions[51] = ChessPieceType::PawnBlack;
    startingPositions[52] = ChessPieceType::PawnBlack;
    startingPositions[53] = ChessPieceType::PawnBlack;
    startingPositions[54] = ChessPieceType::PawnBlack;
    startingPositions[55] = ChessPieceType::PawnBlack;

    startingPositions[56] = ChessPieceType::RookBlack;
    startingPositions[57] = ChessPieceType::KnightBlack;
    startingPositions[58] = ChessPieceType::BishopBlack;
    startingPositions[59] = ChessPieceType::QueenBlack;
    startingPositions[60] = ChessPieceType::KingBlack;
    startingPositions[61] = ChessPieceType::BishopBlack;
    startingPositions[62] = ChessPieceType::KnightBlack;
    startingPositions[63] = ChessPieceType::RookBlack;

    return ChessBoard{ startingPositions };
}

int main() {
    constexpr auto BOARD_SQUARE_SIZE = 8;
    constexpr auto SQUARE_PIXEL_SIZE = 120;

    auto window = Window{};
    window.setFramebufferSize(BOARD_SQUARE_SIZE * SQUARE_PIXEL_SIZE, BOARD_SQUARE_SIZE * SQUARE_PIXEL_SIZE);
    window.setTitle("Chess Game");
    window.show();

    auto renderer = Renderer{};
    renderer.initialize(window);

    const auto checkMateImage = Image::create(1, 1, Color8{ 199, 34, 34, 255 });
    const auto underCheckImage = Image::create(1, 1, Color8{ 117, 122, 45, 255 });
    const auto lightSquareImage = Image::create(1, 1, Color8{ 247, 226, 205, 255 });
    const auto darkSquareImage = Image::create(1, 1, Color8{ 133, 104, 76, 255 });
    const auto selectionOverlayImage = Image::create(1, 1, Color8{ 1, 97, 30, 80 });

    auto checkMateTexture = renderer.createTexture(checkMateImage);
    auto lightSquareTexture = renderer.createTexture(lightSquareImage);
    auto darkSquareTexture = renderer.createTexture(darkSquareImage);
    auto selectionOverlayTexture = renderer.createTexture(selectionOverlayImage);
    auto underCheckTexture = renderer.createTexture(underCheckImage);

    auto chessPieceTextures = createChessPieceTextureMap(renderer);
    auto chessBoard = generateStartingPositions();

    auto gridSelection = GridSelection{};

    while (!window.isCloseRequested()) {
        window.poll();

        if (window.isKeyPressed(KeyboardKeyType::Space)) {
            chessBoard = generateStartingPositions();
        }

        for (auto index = 0; index < chessBoard.board.size(); index++) {
            auto& chessPiece = chessBoard.board[index];
            auto [row, column] = chessBoard.getGridIndicesFromArrayIndex(index);

            const auto yPosition = row * SQUARE_PIXEL_SIZE;
            const auto xPosition = column * SQUARE_PIXEL_SIZE;

            const auto isLightSquare = (row + column) % 2 != 0;

            auto squareTexture = 0;
            if (chessBoard.isPlayerUnderCheck(PlayerTurn::White) && chessPiece == ChessPieceType::KingWhite && chessBoard.isCheckMate) {
                squareTexture = checkMateTexture;
            } else if (chessBoard.isPlayerUnderCheck(PlayerTurn::Black) && chessPiece == ChessPieceType::KingBlack && chessBoard.isCheckMate) {
                squareTexture = checkMateTexture;
            } else if (chessBoard.isPlayerUnderCheck(PlayerTurn::White) && chessPiece == ChessPieceType::KingWhite) {
                squareTexture = underCheckTexture;
            } else if (chessBoard.isPlayerUnderCheck(PlayerTurn::Black) && chessPiece == ChessPieceType::KingBlack) {
                squareTexture = underCheckTexture;
            } else if (isLightSquare) {
                squareTexture = lightSquareTexture;
            } else {
                squareTexture = darkSquareTexture;
            }

            renderer.draw(xPosition, yPosition, SQUARE_PIXEL_SIZE, SQUARE_PIXEL_SIZE, squareTexture);

            if (chessPiece != ChessPieceType::None) {
                const auto chessPieceTexture = chessPieceTextures[chessPiece];
                renderer.draw(xPosition, yPosition, SQUARE_PIXEL_SIZE, SQUARE_PIXEL_SIZE, chessPieceTexture);
            }
        }

        auto cursorPosition = window.getCursorPosition();
        auto [cursorRow, cursorColumn] = chessBoard.getGridIndicesFromScreenPosition(cursorPosition.x, cursorPosition.y);

        const auto cursorRowYPosition = cursorRow * SQUARE_PIXEL_SIZE;
        const auto cursorRowXPosition = cursorColumn * SQUARE_PIXEL_SIZE;
        renderer.draw(cursorRowXPosition, cursorRowYPosition, SQUARE_PIXEL_SIZE, SQUARE_PIXEL_SIZE, selectionOverlayTexture);

        if (gridSelection.piece.type != ChessPieceType::None) {
            const auto selectedChessPieceYPosition = gridSelection.piece.position.row * SQUARE_PIXEL_SIZE;
            const auto selectedChessPieceXPosition = gridSelection.piece.position.column * SQUARE_PIXEL_SIZE;
            renderer.draw(selectedChessPieceXPosition, selectedChessPieceYPosition, SQUARE_PIXEL_SIZE, SQUARE_PIXEL_SIZE, selectionOverlayTexture);

            for (const auto& movementSquare : gridSelection.movementSquares) {
                const auto movementSquareYPosition = movementSquare.row * SQUARE_PIXEL_SIZE;
                const auto movementSquareXPosition = movementSquare.column * SQUARE_PIXEL_SIZE;
                renderer.draw(movementSquareXPosition, movementSquareYPosition, SQUARE_PIXEL_SIZE, SQUARE_PIXEL_SIZE, selectionOverlayTexture);
            }
        }

        const auto windowEvents = window.getPendingEvents();
        for (const auto& windowEvent : windowEvents) {
            if (windowEvent.is<WindowResizeEndEvent>()) {
                const auto& resizeEvent = windowEvent.getData<WindowResizeEndEvent>();
                renderer.resize(resizeEvent.width, resizeEvent.height);
            }

            if (!windowEvent.is<MouseButtonReleaseEvent>()) {
                continue;
            }

            const auto& mouseButtonReleaseEvent = windowEvent.getData<MouseButtonReleaseEvent>();
            if (mouseButtonReleaseEvent.buttonType != MouseButtonType::Left) {
                continue;
            }

            const auto cursorGridIndices = GridIndices{ cursorRow, cursorColumn };

            if (gridSelection.piece.type != ChessPieceType::None) {
                auto movementSquare = std::find(gridSelection.movementSquares.begin(), gridSelection.movementSquares.end(), cursorGridIndices);
                if (movementSquare != gridSelection.movementSquares.end()) {
                    chessBoard.movePieceUnchecked(gridSelection.piece, *movementSquare);
                    gridSelection.piece.type = ChessPieceType::None;
                    break;
                }
            }

            const auto [selectedRow, selectedColumn] = gridSelection.piece.position;
            if (gridSelection.piece.type != ChessPieceType::None && cursorRow == selectedRow && cursorColumn == selectedColumn) {
                gridSelection.piece.type = ChessPieceType::None;
                break;
            }

            const auto squareChessPieceType = chessBoard.getSquareFromGridIndices(cursorGridIndices);
            if (squareChessPieceType != ChessPieceType::None && isPlayerTurnPiece(squareChessPieceType, chessBoard.turn)) {
                gridSelection.piece.type = squareChessPieceType;
                gridSelection.piece.position.column = cursorColumn;
                gridSelection.piece.position.row = cursorRow;
                gridSelection.movementSquares = chessBoard.getPieceMovementSquares(gridSelection.piece, chessBoard.turn);
            }
        }

        renderer.display();
    }
}
