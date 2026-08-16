/**
 * Board.hpp  -  Cascade
 *
 * The playfield grid and the rules that read it.
 *
 * Ten columns by twenty-two rows, where the top two rows sit above the
 * visible area. Guideline Tetris spawns pieces in that buffer so a
 * piece has somewhere to be before it enters play, and topping out is
 * defined as being unable to place it there.
 */

#ifndef TETRIS_BOARD_HPP
#define TETRIS_BOARD_HPP

#include "Tetromino.hpp"

namespace tetris
{
    inline constexpr int BOARD_W    = 10;
    inline constexpr int BOARD_H    = 22;   // including the spawn buffer
    inline constexpr int HIDDEN_ROWS = 2;
    inline constexpr int VISIBLE_H  = BOARD_H - HIDDEN_ROWS;

    /*
     * What a rotation produced, for T-spin scoring.
     */
    enum class SpinResult { NONE, MINI, FULL };

    class Board
    {
    private:
        std::array<PieceType, BOARD_W * BOARD_H> m_cells{};

        [[nodiscard]] std::size_t index(int x, int y) const
        {
            return static_cast<std::size_t>(y * BOARD_W + x);
        }

    public:
        Board();

        void clear();

        [[nodiscard]] bool in_bounds(int x, int y) const;

        /*
         * Cells above the board count as empty; cells outside it
         * horizontally or below it count as solid. That asymmetry is
         * what lets a piece spawn partly above the field.
         */
        [[nodiscard]] bool is_solid(int x, int y) const;

        [[nodiscard]] PieceType at(int x, int y) const;
        void set(int x, int y, PieceType type);

        /*
         * True if the piece can occupy its current position.
         */
        [[nodiscard]] bool fits(const Piece& piece) const;

        /*
         * Stamps the piece into the grid.
         */
        void lock(const Piece& piece);

        /*
         * How many rows the piece can fall before it would collide.
         */
        [[nodiscard]] int drop_distance(const Piece& piece) const;

        /*
         * The piece dropped as far as it will go -- the ghost.
         */
        [[nodiscard]] Piece ghost_of(const Piece& piece) const;

        [[nodiscard]] bool row_full(int y) const;
        [[nodiscard]] bool row_empty(int y) const;

        /*
         * Indices of every complete row, top to bottom.
         */
        [[nodiscard]] std::vector<int> full_rows() const;

        /*
         * Removes the given rows and drops everything above them down.
         */
        void clear_rows(const std::vector<int>& rows);

        /*
         * Standard three-corner T-spin test, run immediately after a
         * rotation that used a kick. A T-spin needs three of the four
         * corners around the T's centre filled; which two of them are
         * the "front" corners decides full versus mini.
         */
        [[nodiscard]] SpinResult detect_spin(const Piece& piece,
                                             bool last_move_was_rotation,
                                             bool used_kick) const;

        /*
         * Highest occupied row, for the danger warning. Returns BOARD_H
         * when the board is empty.
         */
        [[nodiscard]] int highest_filled_row() const;
    };
}

#endif
