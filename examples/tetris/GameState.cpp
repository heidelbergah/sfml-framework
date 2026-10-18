/**
 * GameState.cpp  -  Tetris
 */

#include "GameState.hpp"

namespace tetris
{

ClearResult GameState::apply(int cleared_lines, SpinResult spin, bool perfect_clear)
{
    ClearResult result;
    result.lines         = cleared_lines;
    result.spin          = spin;
    result.perfect_clear = perfect_clear;

    ++pieces_placed;

    // A lock that clears nothing breaks the combo chain but leaves the
    // back-to-back chain alone, only a non-difficult *clear* breaks that.
    if(cleared_lines == 0 && spin == SpinResult::NONE)
    {
        combo = -1;
        return result;
    }

    int base = 0;

    if(spin == SpinResult::FULL)
    {
        switch(cleared_lines)
        {
            case 0:  base = 400;  result.label = "T-SPIN";        break;
            case 1:  base = 800;  result.label = "T-SPIN SINGLE"; break;
            case 2:  base = 1200; result.label = "T-SPIN DOUBLE"; break;
            default: base = 1600; result.label = "T-SPIN TRIPLE"; break;
        }
        ++tspins;
    }
    else if(spin == SpinResult::MINI)
    {
        switch(cleared_lines)
        {
            case 0:  base = 100; result.label = "T-SPIN MINI";        break;
            case 1:  base = 200; result.label = "T-SPIN MINI SINGLE"; break;
            default: base = 400; result.label = "T-SPIN MINI DOUBLE"; break;
        }
        ++tspins;
    }
    else
    {
        switch(cleared_lines)
        {
            case 1:  base = 100; result.label = "SINGLE"; break;
            case 2:  base = 300; result.label = "DOUBLE"; break;
            case 3:  base = 500; result.label = "TRIPLE"; break;
            case 4:  base = 800; result.label = "TETRIS"; ++tetrises; break;
            default: base = 0;   break;
        }
    }

    // Back-to-back applies when this difficult clear follows another.
    bool const difficult = result.is_difficult();
    if(difficult && back_to_back && cleared_lines > 0)
    {
        base = base * 3 / 2;
        result.back_to_back = true;
    }

    if(cleared_lines > 0)
    {
        back_to_back = difficult;
    }

    // Combo counts consecutive locks that each clear at least one line.
    if(cleared_lines > 0)
    {
        ++combo;
        best_combo = std::max(best_combo, combo);
    }
    else
    {
        combo = -1;
    }

    result.combo = combo;

    int points = base * level;

    if(combo > 0) points += 50 * combo * level;

    if(perfect_clear)
    {
        int pc_bonus = 0;
        switch(cleared_lines)
        {
            case 1:  pc_bonus = 800;  break;
            case 2:  pc_bonus = 1200; break;
            case 3:  pc_bonus = 1800; break;
            case 4:  pc_bonus = 2000; break;
            default: pc_bonus = 0;    break;
        }
        points += pc_bonus * level;
        result.label = "PERFECT CLEAR";
    }

    score += points;
    lines += cleared_lines;

    result.points = points;

    // Ten lines per level, and the level never goes backwards.
    level = std::max(level, 1 + lines / 10);

    return result;
}

}
