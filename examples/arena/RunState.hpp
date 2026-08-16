/**
 * RunState.hpp  -  Ironvale Arena
 *
 * The handful of values that outlive a single scene: score carried into
 * the game-over screen, best score carried across runs.
 *
 * A plain shared struct rather than anything clever. Scenes are
 * constructed once and reused, so anything genuinely per-run gets reset
 * in on_enter().
 */

#ifndef ARENA_RUN_STATE_HPP
#define ARENA_RUN_STATE_HPP

#include <string>

namespace arena
{
    struct RunState
    {
        int         score{0};
        int         best_score{0};
        int         wave{1};
        int         kills{0};
        std::string callsign{"Ironvale"};

        void begin_run()
        {
            score = 0;
            wave  = 1;
            kills = 0;
        }

        void finish_run()
        {
            if(score > best_score) best_score = score;
        }

        [[nodiscard]] bool is_new_best() const
        {
            return score >= best_score && score > 0;
        }
    };

    inline RunState& run_state()
    {
        static RunState state;
        return state;
    }
}

#endif
