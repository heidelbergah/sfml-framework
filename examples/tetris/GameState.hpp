/**
 * GameState.hpp  -  Cascade
 *
 * Scoring, levels and the run summary that outlives the play scene.
 *
 * Scoring follows the Tetris guideline: line clears scale with level,
 * T-spins pay far more than the equivalent flat clear, back-to-back
 * difficult clears get a half bonus, and combos pay per chained piece.
 */

#ifndef TETRIS_GAME_STATE_HPP
#define TETRIS_GAME_STATE_HPP

#include "Board.hpp"

namespace tetris
{
    /*
     * What a single lock achieved. Drives both scoring and which
     * celebratory text appears on screen.
     */
    struct ClearResult
    {
        int         lines{0};
        SpinResult  spin{SpinResult::NONE};
        bool        perfect_clear{false};
        bool        back_to_back{false};
        int         combo{0};
        int         points{0};
        std::string label;          // "TETRIS", "T-SPIN DOUBLE", ...

        [[nodiscard]] bool scored() const { return points > 0; }

        /*
         * Difficult clears -- tetrises and T-spins -- are what keep a
         * back-to-back chain alive.
         */
        [[nodiscard]] bool is_difficult() const
        {
            return lines == 4 || spin != SpinResult::NONE;
        }
    };

    struct GameState
    {
        int score{0};
        int lines{0};
        int level{1};
        int combo{-1};              // -1 means no chain in progress
        int best_combo{0};
        bool back_to_back{false};

        int pieces_placed{0};
        int tetrises{0};
        int tspins{0};

        int best_score{0};
        int best_lines{0};

        void begin_run()
        {
            score = 0;
            lines = 0;
            level = 1;
            combo = -1;
            best_combo = 0;
            back_to_back = false;
            pieces_placed = 0;
            tetrises = 0;
            tspins = 0;
        }

        void finish_run()
        {
            if(score > best_score) best_score = score;
            if(lines > best_lines) best_lines = lines;
        }

        /*
         * Seconds a piece takes to fall one row, from the guideline
         * curve. Level 1 is just under a second; level 15 is a tenth of
         * that; level 20 is essentially instant.
         */
        [[nodiscard]] float gravity_interval() const
        {
            float const l = static_cast<float>(level - 1);
            float const base = 0.8f - l * 0.007f;
            if(base <= 0.0f) return 0.001f;
            return std::max(0.001f, std::pow(base, l));
        }

        /*
         * Applies a lock's outcome, updating score, combo and level.
         */
        ClearResult apply(int cleared_lines, SpinResult spin, bool perfect_clear);
    };

    inline GameState& game_state()
    {
        static GameState state;
        return state;
    }
}

#endif
