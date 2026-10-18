/**
 * Tetromino.hpp  -  Cascade
 *
 * Piece shapes and Super Rotation System data.
 *
 * Coordinates are y-down to match the screen, so the standard SRS kick
 * tables (which are published y-up) have their y components negated
 * here. Getting that backwards is the classic SRS bug: rotations look
 * right in open space and kick the wrong way against a wall.
 */

#ifndef TETRIS_TETROMINO_HPP
#define TETRIS_TETROMINO_HPP

#include "Palette.hpp"

namespace tetris
{
    enum class PieceType { I = 0, O, T, S, Z, J, L, COUNT, NONE };

    inline constexpr int PIECE_COUNT = static_cast<int>(PieceType::COUNT);

    /*
     * The four cells of a piece, as offsets inside its bounding box.
     */
    using Cells = std::array<sf::Vector2i, 4>;

    /*
     * Side length of the rotation box: 4 for I, 2 for O, 3 for the rest.
     * O never rotates, which is why its box is exactly its own size.
     */
    [[nodiscard]] int box_size(PieceType type);

    /*
     * Cells for a piece in a given rotation (0 = spawn, then clockwise).
     */
    [[nodiscard]] const Cells& piece_cells(PieceType type, int rotation);

    [[nodiscard]] sf::Color piece_color(PieceType type);

    [[nodiscard]] const char* piece_name(PieceType type);

    /*
     * Column the piece spawns at, so it lands centred on the board.
     */
    [[nodiscard]] int spawn_x(PieceType type);

    /*
     * The five offsets to try when rotating, in order. The first is
     * always (0,0) -- the rotation in place.
     */
    using KickTable = std::array<sf::Vector2i, 5>;

    [[nodiscard]] const KickTable& kicks(PieceType type, int from_rotation,
                                         int to_rotation);

    /*
     * A piece in play: type, rotation, and the position of its bounding
     * box's top-left corner in board coordinates.
     */
    struct Piece
    {
        PieceType type{PieceType::NONE};
        int       rotation{0};
        int       x{0};
        int       y{0};

        [[nodiscard]] const Cells& cells() const
        {
            return piece_cells(type, rotation);
        }

        /*
         * Absolute board coordinates of the four filled cells.
         */
        [[nodiscard]] Cells board_cells() const
        {
            Cells result = cells();
            for(sf::Vector2i& cell : result) cell += sf::Vector2i{x, y};
            return result;
        }

        [[nodiscard]] sf::Color color() const { return piece_color(type); }
        [[nodiscard]] bool      valid() const { return type != PieceType::NONE; }
    };

    /*
     * Seven-bag randomiser: shuffles one of each piece and deals the
     * bag before refilling. Guarantees you never wait more than twelve
     * pieces for any given shape, which is what makes guideline Tetris
     * feel fair rather than random.
     */
    class PieceBag
    {
    private:
        std::vector<PieceType> m_bag;

        void refill();

    public:
        PieceBag();

        [[nodiscard]] PieceType next();
        void reset();
    };
}

#endif
