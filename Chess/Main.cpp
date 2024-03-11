#include "Pandora/Windowing/Window.h"
#include "Pandora/Mathematics/Vector.h"
#include "Pandora/Image.h"

#include "Pandora/Graphics/GraphicsDevice.h"
#include "Pandora/Graphics/Texture.h"
#include "Pandora/Graphics/SceneRenderer.h"
#include "Pandora/Graphics/Scene.h"

#include <filesystem>
#include <optional>
#include <compare>
#include <algorithm>
#include <array>
#include <ranges>
#include <optional>
#include <print>
#include <map>

using namespace Pandora;

enum class ChessPieceType : i16 {
    None,
    Queen,
    Rook,
    Bishop,
    Knight,
    Pawn,
    King,
};

static bool isSlidingPiece(ChessPieceType type) {
    using enum ChessPieceType;
    return type == Queen || type == Rook || type == Bishop;
}

enum class ChessPieceColorType : i16 {
    None,
    Black,
    White
};

static ChessPieceColorType mapColorToOpposite(ChessPieceColorType type) {
    using enum ChessPieceColorType;
    return type == Black ? White : Black;
}

struct ChessPiece {
    ChessPieceType type{};
    ChessPieceColorType color{};
    bool isEnPassantable{};
    bool hasMoved{};

    auto operator<=>(const ChessPiece& other) const {
        if (auto comparison = type <=> other.type; comparison != 0) {
            return comparison;
        } else {
            return color <=> other.color;
        }
    }

    auto operator==(const ChessPiece& other) const {
        return type == other.type && color == other.color;
    }
};

namespace ChessPieces {

    constexpr auto None = ChessPiece{ ChessPieceType::None, ChessPieceColorType::None };
    constexpr auto QueenWhite = ChessPiece{ ChessPieceType::Queen, ChessPieceColorType::White };
    constexpr auto QueenBlack = ChessPiece{ ChessPieceType::Queen, ChessPieceColorType::Black };
    constexpr auto RookWhite = ChessPiece{ ChessPieceType::Rook, ChessPieceColorType::White };
    constexpr auto RookBlack = ChessPiece{ ChessPieceType::Rook, ChessPieceColorType::Black };
    constexpr auto BishopWhite = ChessPiece{ ChessPieceType::Bishop, ChessPieceColorType::White };
    constexpr auto BishopBlack = ChessPiece{ ChessPieceType::Bishop, ChessPieceColorType::Black };
    constexpr auto KnightWhite = ChessPiece{ ChessPieceType::Knight, ChessPieceColorType::White };
    constexpr auto KnightBlack = ChessPiece{ ChessPieceType::Knight, ChessPieceColorType::Black };
    constexpr auto PawnWhite = ChessPiece{ ChessPieceType::Pawn, ChessPieceColorType::White };
    constexpr auto PawnBlack = ChessPiece{ ChessPieceType::Pawn, ChessPieceColorType::Black };
    constexpr auto KingWhite = ChessPiece{ ChessPieceType::King, ChessPieceColorType::White };
    constexpr auto KingBlack = ChessPiece{ ChessPieceType::King, ChessPieceColorType::Black };
}

static constexpr u32 BoardSquareSize = 8;
static constexpr u32 BoardSquarePixelSize = 80;
static constexpr usize BoardSquareCount = BoardSquareSize * BoardSquareSize;

static Vector2u mapCursorPositionToGridIndex(Vector2u position) {
    const auto row = std::clamp(position.x / BoardSquarePixelSize, 0u, BoardSquareSize - 1);
    const auto column = std::clamp(position.y / BoardSquarePixelSize, 0u, BoardSquareSize - 1);

    return { row, column };
}

static Vector2u mapArrayIndexToGridIndex(usize index) {
    const auto row = index % BoardSquareSize;
    const auto column = index / BoardSquareSize;

    return{ row, column };
}

static Vector2f mapGridIndexToPosition(Vector2u grid) {
    return { grid.x * BoardSquarePixelSize, grid.y * BoardSquarePixelSize };
}

static Vector2f mapArrayIndexToPosition(usize index) {
    const auto gridIndex = mapArrayIndexToGridIndex(index);
    return mapGridIndexToPosition(gridIndex);
}

static usize mapGridIndexToArrayIndex(Vector2u grid) {
    return static_cast<usize>(grid.y) * BoardSquareSize + grid.x;
}

enum class DirectionType {
    Up,
    Down,
    Left,
    Right,

    UpLeft,
    UpRight,
    DownRight,
    DownLeft,

    Count
};

enum class KnightDirectionType {
    UpRightRight,
    UpRightUp,
    UpLeftLeft,
    UpLeftUp,

    DownRightRight,
    DownRightDown,
    DownLeftLeft,
    DownLeftDown,

    Count
};

static bool isDirectionAvailableForChessPieceType(DirectionType directionType, ChessPieceType chessPieceType) {
    using enum DirectionType;
    using enum ChessPieceType;

    switch (chessPieceType) {
    case Queen:
        return true;
    case Bishop:
        return directionType == UpLeft 
            || directionType == UpRight 
            || directionType == DownRight 
            || directionType == DownLeft;
    case Rook:
        return directionType == Left
            || directionType == Right
            || directionType == Up
            || directionType == Down;
    default:
        throw std::runtime_error("Unexpected input");
    }
}

static int mapDirectionTypeToArrayIndexOffset(DirectionType type) {
    using enum DirectionType;

    switch (type) {
    case Up:
        return -8;
    case UpRight:
        return -7;
    case UpLeft:
        return -9;
    case Down:
        return 8;
    case DownRight:
        return 9;
    case DownLeft:
        return 7;
    case Left:
        return -1;
    case Right:
        return 1;
    default:
        throw std::runtime_error("Uhandled direction");
    }
}

static int mapKnightDirectionTypeToArrayIndexOffset(KnightDirectionType type) {
    using enum KnightDirectionType;

    switch (type) {
    case UpRightRight:
        return -6;
    case UpRightUp:
        return -15;
    case UpLeftLeft:
        return -10;
    case UpLeftUp:
        return -17;
    case DownRightRight:
        return 10;
    case DownRightDown:
        return 17;
    case DownLeftLeft:
        return 6;
    case DownLeftDown:
        return 15;
    default:
        throw std::runtime_error("Uhandled direction");
    }
}

static usize mapArrayIndexToSquaresToEdge(usize index, DirectionType type) {
    const auto gridIndex = mapArrayIndexToGridIndex(index);
  
    const auto squaresUp = static_cast<usize>(gridIndex.y);
    const auto squaresDown = static_cast<usize>(BoardSquareSize) - 1 - gridIndex.y;
    const auto squaresLeft = static_cast<usize>(gridIndex.x);
    const auto squaresRight = static_cast<usize>(BoardSquareSize) - 1 - gridIndex.x;

    const auto squaresUpLeft = std::min(squaresLeft, squaresUp);
    const auto squaresUpRight = std::min(squaresRight, squaresUp);

    const auto squaresDownLeft = std::min(squaresLeft, squaresDown);
    const auto squaresDownRight = std::min(squaresRight, squaresDown);

    using enum DirectionType;

    switch (type) {
    case Up:
        return squaresUp;
    case UpRight:
        return squaresUpRight;
    case UpLeft:
        return squaresUpLeft;
    case Down:
        return squaresDown;
    case DownRight:
        return squaresDownRight;
    case DownLeft:
        return squaresDownLeft;
    case Left:
        return squaresLeft;
    case Right:
        return squaresRight;
    default:
        throw std::runtime_error("Uhandled direction");
    }
}

static ChessPiece mapFileNameToChessPiece(std::string_view name) {
    if (name.starts_with("BishopWhite")) {
        return ChessPieces::BishopWhite;
    } else if (name.starts_with("RookWhite")) {
        return ChessPieces::RookWhite;
    } else if (name.starts_with("QueenWhite")) {
        return ChessPieces::QueenWhite;
    } else if (name.starts_with("KnightWhite")) {
        return ChessPieces::KnightWhite;
    } else if (name.starts_with("PawnWhite")) {
        return ChessPieces::PawnWhite;
    } else if (name.starts_with("KingWhite")) {
        return ChessPieces::KingWhite;
    } else if (name.starts_with("BishopWhite")) {
        return ChessPieces::BishopWhite;
    } else if (name.starts_with("RookBlack")) {
        return ChessPieces::RookBlack;
    } else if (name.starts_with("QueenBlack")) {
        return ChessPieces::QueenBlack;
    } else if (name.starts_with("KnightBlack")) {
        return ChessPieces::KnightBlack;
    } else if (name.starts_with("PawnBlack")) {
        return ChessPieces::PawnBlack;
    } else if (name.starts_with("KingBlack")) {
        return ChessPieces::KingBlack;
    } else if (name.starts_with("BishopBlack")) {
        return ChessPieces::BishopBlack;
    } else {
        throw std::runtime_error("Unknown chess piece image name");
    }
}

struct ChessMove {
    usize startingSquareIndex{};
    usize targetSquareIndex{};
    bool isCastling{};
    bool isEnPassant{};
    bool isDoubleMovement{};
};

using ChessBoard = std::array<ChessPiece, BoardSquareCount>;

class PossibleChessMoveGenerator {
public:
    PossibleChessMoveGenerator(const ChessBoard& state, ChessPieceColorType color)
        : _state(state), _color(color) {
    }

    std::vector<ChessMove> computeAvailableMoves() {
        for (auto startingSquareIndex = 0; startingSquareIndex < BoardSquareCount; startingSquareIndex++) {
            auto piece = _state[startingSquareIndex];

            if (piece.color != _color) {
                continue;
            }

            if (isSlidingPiece(piece.type)) {
                _computeSlidingPieceMoves(startingSquareIndex, piece);
            } else if (piece.type == ChessPieceType::Pawn) {
                _computePawnMoves(startingSquareIndex, piece);
            } else if (piece.type == ChessPieceType::King) {
                _computeKingMoves(startingSquareIndex, piece);
            } else if (piece.type == ChessPieceType::Knight) {
                _computeKnightMoves(startingSquareIndex);
            }
        }

        return _moves;
    }
private:
    void _computeKnightMoves(i32 startingIndex) {
        const auto directionCount = static_cast<usize>(KnightDirectionType::Count);
        for (auto directionIndex = 0; directionIndex < directionCount; directionIndex++) {
            const auto direction = static_cast<KnightDirectionType>(directionIndex);

            const auto directionOffset = mapKnightDirectionTypeToArrayIndexOffset(direction);
            const auto directionSquareIndex = startingIndex + directionOffset;

            if (!_isKnightDirectionAvailable(startingIndex, direction)) {
                continue;
            }

            const auto pieceIndex = static_cast<usize>(directionSquareIndex);
            const auto& piece = _state[pieceIndex];

            if (piece.color != _color) {
                _moves.emplace_back(startingIndex, static_cast<usize>(pieceIndex));
            }
        }
    }

    bool _isKnightDirectionAvailable(i32 startingIndex, KnightDirectionType direction) {
        const auto leftSquareCount = mapArrayIndexToSquaresToEdge(startingIndex, DirectionType::Left);
        const auto rightSquareCount = mapArrayIndexToSquaresToEdge(startingIndex, DirectionType::Right);
        const auto upSquareCount = mapArrayIndexToSquaresToEdge(startingIndex, DirectionType::Up);
        const auto downSquareCount = mapArrayIndexToSquaresToEdge(startingIndex, DirectionType::Down);

        switch (direction) {
        case KnightDirectionType::UpRightRight:
            return upSquareCount >= 1 && rightSquareCount >= 2;
        case KnightDirectionType::UpRightUp:
            return upSquareCount >= 2 && rightSquareCount >= 1;
        case KnightDirectionType::UpLeftLeft:
            return upSquareCount >= 1 && leftSquareCount >= 2;
        case KnightDirectionType::UpLeftUp:
            return upSquareCount >= 2 && leftSquareCount >= 1;
        case KnightDirectionType::DownRightRight:
            return downSquareCount >= 1 && rightSquareCount >= 2;
        case KnightDirectionType::DownRightDown:
            return downSquareCount >= 2 && rightSquareCount >= 1;
        case KnightDirectionType::DownLeftLeft:
            return downSquareCount >= 1 && leftSquareCount >= 2;
        case KnightDirectionType::DownLeftDown:
            return downSquareCount >= 2 && leftSquareCount >= 1;
        }
    }

    void _computeKingMoves(i32 startingIndex, ChessPiece piece) {
        const auto directionCount = static_cast<usize>(DirectionType::Count);
        for (auto directionIndex = 0; directionIndex < directionCount; directionIndex++) {
            const auto direction = static_cast<DirectionType>(directionIndex);

            const auto squareCountInDirection = mapArrayIndexToSquaresToEdge(startingIndex, direction);
            if (squareCountInDirection == 0) {
                continue;
            }

            const auto directionArrayIndexOffset = mapDirectionTypeToArrayIndexOffset(direction);

            const auto targetSquareIndex = directionArrayIndexOffset + startingIndex;
            const auto targetSquare = _state[targetSquareIndex];

            if (targetSquare.color != _color) {
                _moves.emplace_back(startingIndex, targetSquareIndex);
            }
        }

        if (!piece.hasMoved) {
            _computeKingCastleInDirection(startingIndex, DirectionType::Left);
            _computeKingCastleInDirection(startingIndex, DirectionType::Right);
        }
    }

    void _computeKingCastleInDirection(i32 startingIndex, DirectionType direction) {
        const auto directionArrayIndexOffset = mapDirectionTypeToArrayIndexOffset(direction);
        const auto squareCountInDirection = mapArrayIndexToSquaresToEdge(startingIndex, direction);

        for (auto directionSquareIndex = 1; directionSquareIndex <= squareCountInDirection; directionSquareIndex++) {
            const auto targetSquareIndex = directionSquareIndex * directionArrayIndexOffset + startingIndex;
            const auto targetSquare = _state[targetSquareIndex];

            if (targetSquare.type == ChessPieceType::Rook && targetSquare.color == _color && !targetSquare.hasMoved) {
                _moves.emplace_back(startingIndex, 2 * directionArrayIndexOffset + startingIndex, true);
                break;
            } 

            if (targetSquare != ChessPieces::None) {
                break;
            }
        }
    }

    void _computePawnMoves(i32 startingIndex, ChessPiece piece) {
        using enum ChessPieceColorType;
        using enum DirectionType;

        const auto pawnVerticalDirection = piece.color == Black ? Down : Up;
        const auto pawnDiagonalLeftDirection = piece.color == Black ? DownLeft : UpLeft;
        const auto pawnDiagonalRightDirection = piece.color == Black ? DownRight : UpRight;

        _computePawnVerticalMoves(startingIndex, piece.hasMoved, pawnVerticalDirection);
        _computePawnDiagonalMoves(startingIndex, pawnDiagonalLeftDirection);
        _computePawnDiagonalMoves(startingIndex, pawnDiagonalRightDirection);
        _computePawnEnPassantInDirection(startingIndex, piece, DirectionType::Left);
        _computePawnEnPassantInDirection(startingIndex, piece, DirectionType::Right);
    }

    void _computePawnEnPassantInDirection(i32 startingIndex, ChessPiece piece, DirectionType direction) {
        using enum ChessPieceColorType;
        using enum DirectionType;

        const auto squareCountInDirection = mapArrayIndexToSquaresToEdge(startingIndex, direction);
        const auto directionArrayIndexOffset = mapDirectionTypeToArrayIndexOffset(direction);

        if (squareCountInDirection == 0) {
            return;
        }

        const auto targetSquareIndex = directionArrayIndexOffset + startingIndex;
        const auto targetSquare = _state[targetSquareIndex];

        if (targetSquare.isEnPassantable) {
            const auto pawnVerticalDirection = piece.color == Black ? Down : Up;
            const auto pawnVerticalDirectionOffset = mapDirectionTypeToArrayIndexOffset(pawnVerticalDirection);
            _moves.emplace_back(startingIndex, targetSquareIndex + pawnVerticalDirectionOffset, false, true);
        }
    }

    void _computePawnVerticalMoves(i32 startingIndex, bool hasMoved, DirectionType direction) {
        const auto maximumPawnMoveCount = hasMoved ? 1ull : 2ull;

        const auto squareCountInDirection = mapArrayIndexToSquaresToEdge(startingIndex, direction);
        const auto directionArrayIndexOffset = mapDirectionTypeToArrayIndexOffset(direction);

        const auto maximumDirectionSquareIndex = std::min(maximumPawnMoveCount, squareCountInDirection);

        for (auto directionSquareIndex = 1; directionSquareIndex <= maximumDirectionSquareIndex; directionSquareIndex++) {
            const auto targetSquareIndex = directionSquareIndex * directionArrayIndexOffset + startingIndex;
            const auto targetSquare = _state[targetSquareIndex];

            if (targetSquare != ChessPieces::None) {
                break;
            }

            bool isDoubleMovement = directionSquareIndex == 2ull;
            _moves.emplace_back(startingIndex, targetSquareIndex, false, false, isDoubleMovement);
        }
    }

    void _computePawnDiagonalMoves(i32 startingIndex, DirectionType direction) {
        const auto squareCountInDirection = mapArrayIndexToSquaresToEdge(startingIndex, direction);

        if (squareCountInDirection == 0) {
            return;
        }

        const auto directionArrayIndexOffset = mapDirectionTypeToArrayIndexOffset(direction);

        const auto targetSquareIndex = directionArrayIndexOffset + startingIndex;
        const auto targetSquare = _state[targetSquareIndex];

        if (targetSquare.color == mapColorToOpposite(_color)) {
            _moves.emplace_back(startingIndex, targetSquareIndex);
        }
    }

    void _computeSlidingPieceMoves(i32 startingIndex, ChessPiece piece) {
        const auto directionCount = static_cast<usize>(DirectionType::Count);
        for (auto directionIndex = 0; directionIndex < directionCount; directionIndex++) {
            const auto direction = static_cast<DirectionType>(directionIndex);

            if (!isDirectionAvailableForChessPieceType(direction, piece.type)) {
                continue;
            }

            const auto squaresInDirection = mapArrayIndexToSquaresToEdge(startingIndex, direction);
            for (auto directionSquareIndex = 1; directionSquareIndex <= squaresInDirection; directionSquareIndex++) {
                const auto targetSquareIndex = directionSquareIndex * mapDirectionTypeToArrayIndexOffset(direction) + startingIndex;
                const auto targetSquare = _state[targetSquareIndex];

                if (targetSquare.color == _color) {
                    break;
                }

                _moves.emplace_back(startingIndex, targetSquareIndex);

                if (targetSquare.color == mapColorToOpposite(_color)) {
                    break;
                }
            }
        }
    }
private:
    ChessBoard _state{};
    ChessPieceColorType _color{};

    std::vector<ChessMove> _moves{};
};

class ChessGame {
public:
    void onSetup(Window& window) {
        const auto size = BoardSquarePixelSize * BoardSquareSize;

        window.setFramebufferSize(size, size);
        window.setResizeable(false);
        window.setTitle("Chess Game");
        window.show();
    }

    void onResourceLoad(GraphicsDevice& device) {
        _playerColorTurn = ChessPieceColorType::White;

        _loadChessPieceSprites(device);
        _loadStaticSprites(device);

        _resetGameState();
    }

    void onUpdate(Window& window) {
        if (window.isKeyPressed(KeyboardKeyType::Esc)) {
            _resetGameState();
        }

        _cursorPosition = window.getCursorPosition();

        for (const auto& event : window.getPendingEvents()) {
            if (event.is<MouseButtonPressedEvent>()) {
                const auto gridIndex = mapCursorPositionToGridIndex(_cursorPosition);
                const auto pieceIndex = mapGridIndexToArrayIndex(gridIndex);

                auto& piece = _pieces[pieceIndex];
                if (piece != ChessPieces::None) {
                    _movingPieceOriginalIndex = pieceIndex;
                    _movingPiece = piece;

                    _selectedPieceGridIndex = mapCursorPositionToGridIndex(_cursorPosition);
                    _selectedPiece = piece;

                    piece = ChessPieces::None;
                }
            } else if (event.is<MouseButtonReleaseEvent>() && _movingPiece != ChessPieces::None) {
                if (_movingPiece != ChessPieces::None) {
                    const auto cursorGridIndex = mapCursorPositionToGridIndex(_cursorPosition);
                    const auto cursorPieceIndex = mapGridIndexToArrayIndex(cursorGridIndex);

                    const auto isDropValid = [cursorPieceIndex](const ChessMove& move) { return move.targetSquareIndex == cursorPieceIndex; };

                    auto movesForSelectedPiece = _getMovesForSelectedPiece();
                    auto move = std::ranges::find_if(movesForSelectedPiece, isDropValid);

                    if (move != movesForSelectedPiece.cend()) {
                        auto& cursorPiece = _pieces[cursorPieceIndex];

                        _selectedPiece = ChessPieces::None;
                        _isDeselectPossible = false;
                        _isKingUnderCheck = false;

                        cursorPiece = _movingPiece;
                        cursorPiece.hasMoved = true;

                        bool isPawnPromotionPossible = cursorGridIndex.y == 0 || cursorGridIndex.y == BoardSquareSize - 1;
                        if (_movingPiece.type == ChessPieceType::Pawn && isPawnPromotionPossible) {
                            cursorPiece.type = ChessPieceType::Queen;
                        }

                        if (move->isCastling) {
                            const auto castlingDirection = _mapTargetIndexToCastlingDirection(move->targetSquareIndex);
                            const auto rookPieceIndex = _findCastlingRookInDirection(move->startingSquareIndex, castlingDirection);
                            const auto offset = mapDirectionTypeToArrayIndexOffset(castlingDirection);

                            _pieces[cursorPieceIndex - offset] = _pieces[rookPieceIndex];
                            _pieces[cursorPieceIndex - offset].hasMoved = true;
                            _pieces[rookPieceIndex] = ChessPieces::None;
                        }

                        if (move->isDoubleMovement) {
                            using enum DirectionType;

                            bool isPawnToTheLeft = _isPawnInDirection(cursorPieceIndex, Left);
                            bool isPawnToTheRight = _isPawnInDirection(cursorPieceIndex, Right);

                            cursorPiece.isEnPassantable = isPawnToTheLeft || isPawnToTheRight;
                        }

                        if (move->isEnPassant) {
                            using enum ChessPieceColorType;
                            using enum DirectionType;

                            const auto enPassantCaptureDirection = _playerColorTurn == Black ? Up : Down;
                            const auto offset = mapDirectionTypeToArrayIndexOffset(enPassantCaptureDirection);

                            _pieces[cursorPieceIndex + offset] = ChessPieces::None;
                        }

                        _isKingUnderCheck = _computeKingUnderCheck();
                        _movesHistory.push_back(*move);
                        _playerColorTurn = mapColorToOpposite(_playerColorTurn);

                        _computeAvailableMoves();

                        if (_isKingUnderCheck && _legalMoves.empty()) {
                            _isKingUnderMate = true;
                        } else if (_legalMoves.empty()) {
                            _isKingUnderDraw = true;
                        }
                    } else if (cursorPieceIndex == _movingPieceOriginalIndex && _isDeselectPossible) {
                        _selectedPiece = ChessPieces::None;
                        _pieces[_movingPieceOriginalIndex] = _movingPiece;
                        _isDeselectPossible = false;
                    } else {
                        _pieces[_movingPieceOriginalIndex] = _movingPiece;
                        _isDeselectPossible = true;
                    }

                    _movingPiece = ChessPieces::None;
                }
            }
        }
    }

    void onDraw(Scene& scene) {
        for (auto index = 0; index < _pieces.size(); index++) {
            const auto gridIndex = mapArrayIndexToGridIndex(index);
            const auto position = mapGridIndexToPosition(gridIndex);

            const auto piece = _pieces[index];

            auto gridSprite = _getBoardSquareSprite(gridIndex, piece);
            gridSprite.position = position;

            scene.sprites.push_back(gridSprite);

            if (piece != ChessPieces::None) {
                auto pieceSprite = _chessPieceSprites[piece];
                pieceSprite.position = position;

                scene.sprites.push_back(pieceSprite);
            }            
        }

        const auto cursorGridIndex = mapCursorPositionToGridIndex(_cursorPosition);
        const auto cursorGridPosition = mapGridIndexToPosition(cursorGridIndex);

        auto hoverSprite = _squareHoverSprite;
        hoverSprite.position = cursorGridPosition;

        scene.sprites.push_back(hoverSprite);

        if (_movingPiece != ChessPieces::None) {
            auto movingPieceSprite = _chessPieceSprites[_movingPiece];
            movingPieceSprite.position = _cursorPosition;
            movingPieceSprite.origin = Vector2f{ BoardSquarePixelSize / 2.f, BoardSquarePixelSize / 2.f };
            movingPieceSprite.zIndex = 6;

            scene.sprites.push_back(movingPieceSprite);
        }

        if (_selectedPiece != ChessPieces::None) {
            auto selectedGridSprite = _squareSelectedSprite;
            selectedGridSprite.position = mapGridIndexToPosition(_selectedPieceGridIndex);
            selectedGridSprite.zIndex = 3;

            scene.sprites.push_back(selectedGridSprite);
        }

        if (!_movesHistory.empty()) {
            auto& lastMove = _movesHistory.back();

            auto moveStartingSquareSprite = _squareSelectedSprite;
            moveStartingSquareSprite.position = mapArrayIndexToPosition(lastMove.startingSquareIndex);

            scene.sprites.push_back(moveStartingSquareSprite);

            auto moveTargetSquareSprite = _squareSelectedSprite;
            moveTargetSquareSprite.position = mapArrayIndexToPosition(lastMove.targetSquareIndex);

            scene.sprites.push_back(moveTargetSquareSprite);
        }

        if (_selectedPiece != ChessPieces::None) {
            for (const ChessMove& move : _getMovesForSelectedPiece()) {
                const auto highlightGridIndex = mapArrayIndexToGridIndex(move.targetSquareIndex);
                const auto highlightPosition = mapGridIndexToPosition(highlightGridIndex);

                auto& targetPiece = _pieces[move.targetSquareIndex];
                if (targetPiece.color == mapColorToOpposite(_playerColorTurn)) {
                    auto highlightCaptureSprite = _highlightCaptureSprite;
                    highlightCaptureSprite.position = highlightPosition;
                    scene.sprites.push_back(highlightCaptureSprite);
                } else {
                    const auto highlightCenteredPosition = BoardSquarePixelSize / 2;

                    const auto highlightX = highlightPosition.x + highlightCenteredPosition;
                    const auto highlightY = highlightPosition.y + highlightCenteredPosition;

                    auto highlightSprite = _highlightSprite;
                    highlightSprite.position = Vector2f{ highlightX, highlightY };
                    scene.sprites.push_back(highlightSprite);
                }
            }
        }
    }
private:
    void _resetGameState() {
        _cursorPosition = {};
        _selectedPiece = {};
        _selectedPieceGridIndex = {};

        _isDeselectPossible = false;
        _isKingUnderCheck = false;
        _isKingUnderMate = false;
        _isKingUnderDraw = false;

        _movingPiece = ChessPieces::None;
        _movingPieceOriginalIndex = 0;

        _playerColorTurn = ChessPieceColorType::White;

        _legalMoves.clear();
        _availableMoves.clear();
        _movesHistory.clear();

        _pieces = {};

        _generateStartingPositions();
        _computeAvailableMoves();
    }

    Sprite _getBoardSquareSprite(const Vector2u& gridIndex, ChessPiece piece) const {
        const auto isLightSquare = (gridIndex.x + gridIndex.y) % 2 != 0;
        const auto isPlayerKing = piece.type == ChessPieceType::King && piece.color == _playerColorTurn;

        if (_isKingUnderCheck && !_isKingUnderMate && isPlayerKing) {
           return _kingUnderCheckSprite;
        } else if (_isKingUnderMate && isPlayerKing) {
            return _kingUnderMateSprite;
        } else if (_isKingUnderDraw) {
            return _kingUnderDrawSprite;
        } else if (isLightSquare) {
            return _lightSquareSprite;
        } else {
            return _darkSquareSprite;
        }
    }

    DirectionType _mapTargetIndexToCastlingDirection(usize targetIndex) {
        if (targetIndex > 60 || (targetIndex > 4 && targetIndex < 7)) {
            return DirectionType::Right;
        } else {
            return DirectionType::Left;
        }
    }

    usize _findCastlingRookInDirection(i32 startingIndex, DirectionType direction) {
        const auto directionArrayIndexOffset = mapDirectionTypeToArrayIndexOffset(direction);
        const auto squareCountInDirection = mapArrayIndexToSquaresToEdge(startingIndex, direction);

        for (auto directionSquareIndex = 1; directionSquareIndex <= squareCountInDirection; directionSquareIndex++) {
            const auto targetSquareIndex = directionSquareIndex * directionArrayIndexOffset + startingIndex;
            const auto targetSquare = _pieces[targetSquareIndex];

            if (targetSquare.type == ChessPieceType::Rook) {
                return targetSquareIndex;
            }
        }
    }

    bool _isPawnInDirection(i32 startingIndex, DirectionType direction) {
        const auto directionArrayIndexOffset = mapDirectionTypeToArrayIndexOffset(direction);
        const auto squareCountInDirection = mapArrayIndexToSquaresToEdge(startingIndex, direction);

        if (squareCountInDirection == 0) {
            return false;
        }

        const auto targetSquareIndex = directionArrayIndexOffset + startingIndex;
        const auto targetSquare = _pieces[targetSquareIndex];

        return targetSquare.type == ChessPieceType::Pawn && targetSquare.color == mapColorToOpposite(_playerColorTurn);
    }

    void _generateStartingPositions() {
        _pieces[0] = ChessPieces::RookBlack;
        _pieces[1] = ChessPieces::KnightBlack;
        _pieces[2] = ChessPieces::BishopBlack;
        _pieces[3] = ChessPieces::QueenBlack;
        _pieces[4] = ChessPieces::KingBlack;
        _pieces[5] = ChessPieces::BishopBlack;
        _pieces[6] = ChessPieces::KnightBlack;
        _pieces[7] = ChessPieces::RookBlack;

        _pieces[56] = ChessPieces::RookWhite;
        _pieces[57] = ChessPieces::KnightWhite;
        _pieces[58] = ChessPieces::BishopWhite;
        _pieces[59] = ChessPieces::QueenWhite;
        _pieces[60] = ChessPieces::KingWhite;
        _pieces[61] = ChessPieces::BishopWhite;
        _pieces[62] = ChessPieces::KnightWhite;
        _pieces[63] = ChessPieces::RookWhite;

        std::fill(&_pieces[8], &_pieces[16], ChessPieces::PawnBlack);
        std::fill(&_pieces[48], &_pieces[56], ChessPieces::PawnWhite);
    }

    auto _getMovesForSelectedPiece() {
        const auto selectedPieceArrayIndex = mapGridIndexToArrayIndex(_selectedPieceGridIndex);
        auto isSelectedPieceMove = [selectedPieceArrayIndex](const ChessMove& move) { 
            return move.startingSquareIndex == selectedPieceArrayIndex; 
        };

        return _legalMoves | std::views::filter(isSelectedPieceMove);
    }

    bool _computeKingUnderCheck() {
        auto generator = PossibleChessMoveGenerator{ _pieces, _playerColorTurn };
        const auto possibleMoves = generator.computeAvailableMoves();

        const auto isKingUnderThreat = [this](const ChessMove& move) {
            const auto piece = _pieces[move.targetSquareIndex];
            return piece.type == ChessPieceType::King && piece.color == mapColorToOpposite(_playerColorTurn);
        };

        return std::ranges::any_of(possibleMoves, isKingUnderThreat);
    }

    void _computeAvailableMoves() {
        _legalMoves.clear();

        auto generator = PossibleChessMoveGenerator{ _pieces, _playerColorTurn };
        _availableMoves = generator.computeAvailableMoves();

        for (const auto& move : _availableMoves) {
            const auto startingPiece = _pieces[move.startingSquareIndex];
            const auto targetPiece = _pieces[move.targetSquareIndex];

            _pieces[move.startingSquareIndex] = ChessPieces::None;
            _pieces[move.targetSquareIndex] = startingPiece;

            auto generator = PossibleChessMoveGenerator{ _pieces, mapColorToOpposite(_playerColorTurn) };
            const auto possibleOpponentMoves = generator.computeAvailableMoves();

            const auto isKingCaptured = [this](const ChessMove& move) {
                const auto piece = _pieces[move.targetSquareIndex];
                return piece.type == ChessPieceType::King && piece.color == _playerColorTurn;
            };

            if (!std::ranges::any_of(possibleOpponentMoves, isKingCaptured)) {
                _legalMoves.push_back(move);
            }

            _pieces[move.startingSquareIndex] = startingPiece;
            _pieces[move.targetSquareIndex] = targetPiece;
        }
    }

    void _loadStaticSprites(GraphicsDevice& device) {
        _lightSquareSprite.texture = Texture{ device, Image::create(1, 1, Color8{ 240, 245, 223 }) };
        _lightSquareSprite.scale = Vector2f{ BoardSquarePixelSize };

        _darkSquareSprite.texture = Texture{ device, Image::create(1, 1, Color8{ 95, 148, 92 }) };
        _darkSquareSprite.scale = Vector2f{ BoardSquarePixelSize };

        _squareSelectedSprite.texture = Texture{ device, Image::create(1, 1, Color8{ 181, 184, 50, 150 }) };
        _squareSelectedSprite.scale = Vector2f{ BoardSquarePixelSize };

        _squareHoverSprite.texture = Texture{ device, _generateBorderImage(4, Color8{ 178, 209, 189 }) };
        _squareHoverSprite.zIndex = 5;

        _kingUnderCheckSprite.texture = Texture{ device, Image::create(1, 1, Color8{ 191, 90, 91 }) };
        _kingUnderCheckSprite.scale = Vector2f{ BoardSquarePixelSize };

        _kingUnderMateSprite.texture = Texture{ device, Image::create(1, 1, Color8{ 138, 3, 5 }) };
        _kingUnderMateSprite.scale = Vector2f{ BoardSquarePixelSize };

        _kingUnderDrawSprite.texture = Texture{ device, Image::create(1, 1, Color8{ 177, 189, 11 }) };
        _kingUnderDrawSprite.scale = Vector2f{ BoardSquarePixelSize };


        const auto higlightSizePixels = 30;

        _highlightSprite.texture = Texture{ device, _generateHighlightImage(higlightSizePixels, Color8{ 92, 92, 92, 70 }) };
        _highlightSprite.origin = Vector2f{ higlightSizePixels / 2 };
        _highlightSprite.zIndex = 5;

        _highlightCaptureSprite.texture = Texture{ device, _generateHighlightCaptureImage(10, Color8{ 92, 92, 92, 90 }) };
        _highlightCaptureSprite.zIndex = 5;
    }

    Image _generateBorderImage(i32 borderWidthPixels, Color8 borderColor) {
        auto borderImage = Image::create(BoardSquarePixelSize, BoardSquarePixelSize, {});

        for (auto x = 0; x < BoardSquarePixelSize; x++) {
            for (auto y = 0; y < BoardSquarePixelSize; y++) {
                if (x < borderWidthPixels || x >= BoardSquarePixelSize - borderWidthPixels) {
                    borderImage.setPixel(x, y, borderColor);
                } else if (y < borderWidthPixels || y >= BoardSquarePixelSize - borderWidthPixels) {
                    borderImage.setPixel(x, y, borderColor);
                }
            }
        }

        return borderImage;
    }

    Image _generateHighlightImage(i32 size, Color8 color) {
        auto higlightImage = Image::create(size, size, {});

        const auto radius = size / 2;
        const auto isInsideCircle = [radius](i32 x, i32 y) {
            const auto xOffset = x - radius;
            const auto yOffset = y - radius;
            return xOffset * xOffset + yOffset * yOffset < radius * radius;
        };

        for (auto x = 0; x < size; x++) {
            for (auto y = 0; y < size; y++) {
                if (isInsideCircle(x, y)) {
                    higlightImage.setPixel(x, y, color);
                }
            }
        }

        return higlightImage;
    }

    Image _generateHighlightCaptureImage(i32 borderSize, Color8 color) {
        auto higlightImage = Image::create(BoardSquarePixelSize, BoardSquarePixelSize, {});

        const auto outerCircleRadius = BoardSquarePixelSize / 2;
        const auto innerCircleRadius = BoardSquarePixelSize / 2 - borderSize / 2;
        const auto centerPosition = BoardSquarePixelSize / 2;

        const auto isInsideCircle = [centerPosition](i32 radius, i32 x, i32 y) {
            const auto xOffset = x - centerPosition;
            const auto yOffset = y - centerPosition;
            return xOffset * xOffset + yOffset * yOffset < radius * radius;
        };

        for (auto x = 0; x < BoardSquarePixelSize; x++) {
            for (auto y = 0; y < BoardSquarePixelSize; y++) {
                if (isInsideCircle(outerCircleRadius, x, y) && !isInsideCircle(innerCircleRadius, x, y)) {
                    higlightImage.setPixel(x, y, color);
                }
            }
        }

        return higlightImage;
    }

    void _loadChessPieceSprites(GraphicsDevice& device) {
        const auto chessPieceDirectoryPath = std::filesystem::path{ "./Assets/ChessPieces" };
        if (!std::filesystem::exists(chessPieceDirectoryPath)) {
            throw std::runtime_error("Asset folder does not exist");
        }

        for (const auto& entry : std::filesystem::directory_iterator{ chessPieceDirectoryPath }) {
            const auto& imagePath = entry.path();

            auto chessPieceSprite = Sprite{};
            chessPieceSprite.texture = Texture{ device, Image::load(imagePath) };
            chessPieceSprite.scale = Vector2f{ Vector2f{ BoardSquarePixelSize } / chessPieceSprite.texture.getSize() };
            chessPieceSprite.zIndex = 4;

            const auto chessPiece = mapFileNameToChessPiece(imagePath.filename().string());
            _chessPieceSprites.emplace(chessPiece, chessPieceSprite);
        }
    }

    std::array<ChessPiece, BoardSquareCount> _pieces{};

    Sprite _lightSquareSprite{};
    Sprite _darkSquareSprite{};

    Sprite _squareHoverSprite{};
    Sprite _squareSelectedSprite{};

    Sprite _highlightSprite{};
    Sprite _highlightCaptureSprite{};

    Sprite _kingUnderCheckSprite{};
    Sprite _kingUnderMateSprite{};
    Sprite _kingUnderDrawSprite{};

    Vector2u _cursorPosition{};

    ChessPiece _selectedPiece{};
    Vector2u _selectedPieceGridIndex{};

    bool _isDeselectPossible{};
    bool _isKingUnderCheck{};
    bool _isKingUnderMate{};
    bool _isKingUnderDraw{};

    ChessPiece _movingPiece{};
    usize _movingPieceOriginalIndex{};

    ChessPieceColorType _playerColorTurn{};

    std::map<ChessPiece, Sprite> _chessPieceSprites{};

    std::vector<ChessMove> _legalMoves{};
    std::vector<ChessMove> _availableMoves{};
    std::vector<ChessMove> _movesHistory{};
};

int main() {
    auto game = ChessGame{};

    auto window = Window{};
    game.onSetup(window);

    auto device = GraphicsDevice{};
    device.configure(window);

    game.onResourceLoad(device);

    auto renderer = SceneRenderer{ device };

    auto camera = Camera{};
    camera.size = window.getFramebufferSize();

    renderer.setCamera(device, camera);

    while (!window.isCloseRequested()) {
        window.pollEvents();

        for (const auto& event : window.getPendingEvents()) {
            if (event.is<WindowResizeEndEvent>()) {
                device.configure(window);
            }
        }

        if (!device.isSuspended()) {
            auto scene = Scene{};

            game.onUpdate(window);
            game.onDraw(scene);

            renderer.draw(device, scene);
        }
    }
}
