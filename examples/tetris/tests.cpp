/**
 * tests.cpp  -  Tetris
 *
 * Rule tests. None of this needs a window, so it runs in CI:
 *
 *     make test-tetris
 *
 * The parts worth testing here are the ones you cannot check by looking
 * at the screen: whether the bag is really a bag, whether SRS kicks go
 * the right way against a wall, whether clearing non-adjacent rows
 * drops the survivors by the right amount, and whether scoring matches
 * the guideline.
 */

#include "Board.hpp"
#include "GameState.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <map>
#include <limits>
#include <set>

namespace
{
    int g_checks = 0;
    int g_failures = 0;

    void check(bool condition, const std::string& what)
    {
        ++g_checks;
        std::printf(condition ? "  \033[32mPASS\033[0m  %s\n"
                              : "  \033[31mFAIL\033[0m  %s\n", what.c_str());
        if(!condition) ++g_failures;
    }

    void check_eq(int actual, int expected, const std::string& what)
    {
        if(actual != expected)
            std::printf("        expected %d, got %d\n", expected, actual);
        check(actual == expected, what);
    }

    void section(const char* name)
    {
        std::printf("\n\033[1m%s\033[0m\n", name);
    }
}

using namespace tetris;

// ------------------------------------------------------------- rotation

void test_shapes()
{
    section("Piece shapes and rotation");

    // Every piece is four cells in every rotation. A rotation that drops
    // or duplicates a cell is the kind of bug that only shows up against
    // a wall, hours later.
    bool all_four = true;
    bool in_box   = true;

    for(int t = 0; t < PIECE_COUNT; ++t)
    {
        PieceType const type = static_cast<PieceType>(t);
        int const n = box_size(type);

        for(int r = 0; r < 4; ++r)
        {
            Cells const& cells = piece_cells(type, r);

            std::set<std::pair<int, int>> unique;
            for(sf::Vector2i c : cells)
            {
                unique.insert({c.x, c.y});
                if(c.x < 0 || c.x >= n || c.y < 0 || c.y >= n) in_box = false;
            }
            if(unique.size() != 4) all_four = false;
        }
    }

    check(all_four, "every piece has four distinct cells in all four rotations");
    check(in_box, "every cell stays inside its rotation box");

    // O is rotationally symmetric, so all four states must be identical.
    bool o_static = true;
    for(int r = 1; r < 4; ++r)
    {
        Cells const& a = piece_cells(PieceType::O, 0);
        Cells const& b = piece_cells(PieceType::O, r);
        std::set<std::pair<int, int>> sa, sb;
        for(sf::Vector2i c : a) sa.insert({c.x, c.y});
        for(sf::Vector2i c : b) sb.insert({c.x, c.y});
        if(sa != sb) o_static = false;
    }
    check(o_static, "O is identical in all four rotations");

    // Four rotations must return to the start.
    bool cycles = true;
    for(int t = 0; t < PIECE_COUNT; ++t)
    {
        PieceType const type = static_cast<PieceType>(t);
        std::set<std::pair<int, int>> first, fourth;
        for(sf::Vector2i c : piece_cells(type, 0)) first.insert({c.x, c.y});
        for(sf::Vector2i c : piece_cells(type, 4)) fourth.insert({c.x, c.y});
        if(first != fourth) cycles = false;
    }
    check(cycles, "four rotations returns every piece to spawn state");

    // Spawn orientation of T is stem-up: one cell on the top row,
    // centred over three on the row below.
    {
        Cells const& t = piece_cells(PieceType::T, 0);
        int top_row = 0, bottom_row = 0;
        for(sf::Vector2i c : t) (c.y == 0 ? top_row : bottom_row)++;
        check(top_row == 1 && bottom_row == 3, "T spawns stem-up");
    }

    // I spawns flat and horizontal.
    {
        Cells const& i = piece_cells(PieceType::I, 0);
        bool flat = true;
        for(sf::Vector2i c : i) if(c.y != i[0].y) flat = false;
        check(flat, "I spawns horizontal");
    }
}

// ------------------------------------------------------------------ bag

void test_bag()
{
    section("Seven-bag randomiser");

    Random::seed(4242);
    PieceBag bag;

    bool every_bag_complete = true;

    for(int round = 0; round < 60; ++round)
    {
        std::set<int> seen;
        for(int i = 0; i < 7; ++i) seen.insert(static_cast<int>(bag.next()));
        if(seen.size() != 7) every_bag_complete = false;
    }

    check(every_bag_complete,
          "every consecutive group of seven contains each piece exactly once");

    // Which also bounds the worst-case drought at twelve pieces.
    Random::seed(99);
    PieceBag drought_bag;
    std::map<int, int> last_seen;
    int worst_gap = 0;

    for(int i = 0; i < 700; ++i)
    {
        int const piece = static_cast<int>(drought_bag.next());
        if(last_seen.count(piece)) worst_gap = std::max(worst_gap, i - last_seen[piece]);
        last_seen[piece] = i;
    }

    /*
     * A piece can be first in one bag and last in the next, so the most
     * you ever wait is twelve other pieces, an index gap of thirteen.
     */
    check(worst_gap <= 13, "at most twelve other pieces between repeats");
}

// ---------------------------------------------------------------- board

void test_board()
{
    section("Board");

    Board board;

    check(board.row_empty(5), "a fresh board is empty");

    // Walls and floor are solid; open sky above the board is not. That
    // asymmetry is what lets a piece spawn partly above the field.
    check(board.is_solid(-1, 10),       "left wall is solid");
    check(board.is_solid(BOARD_W, 10),  "right wall is solid");
    check(board.is_solid(0, BOARD_H),   "floor is solid");
    check(!board.is_solid(0, -1),       "sky above the board is not solid");

    // Drop distance and ghost.
    Piece piece{PieceType::O, 0, 4, 0};
    int const distance = board.drop_distance(piece);
    check_eq(distance, BOARD_H - 2, "O dropped on an empty board lands on the floor");

    Piece const ghost = board.ghost_of(piece);
    check_eq(ghost.y, piece.y + distance, "ghost sits at the landing position");

    board.lock(ghost);
    check(board.at(4, BOARD_H - 1) == PieceType::O, "locking stamps into the grid");

    /*
     * Clearing non-adjacent rows. Rows 18 and 20 go; the marker sitting
     * in row 19 has one cleared row beneath it, so it should end up in
     * row 20. The row at 17 has two beneath it and should fall to 19.
     */
    Board gaps;
    for(int x = 0; x < BOARD_W; ++x)
    {
        gaps.set(x, 18, PieceType::I);
        gaps.set(x, 20, PieceType::I);
    }
    gaps.set(0, 19, PieceType::T);       // marker, one below it clears
    gaps.set(0, 17, PieceType::S);       // marker, two below it clear

    std::vector<int> const full = gaps.full_rows();
    check(full.size() == 2 && full[0] == 18 && full[1] == 20,
          "full_rows finds both complete rows, top to bottom");

    gaps.clear_rows(full);

    check(gaps.at(0, 20) == PieceType::T, "a row with one clear beneath it falls one");
    check(gaps.at(0, 19) == PieceType::S, "a row with two clears beneath it falls two");
    check(gaps.at(1, 20) == PieceType::NONE, "the rest of the fallen row stays empty");

    // Clearing must not leave the board taller than it started.
    Board overflow;
    for(int y = BOARD_H - 4; y < BOARD_H; ++y)
        for(int x = 0; x < BOARD_W; ++x) overflow.set(x, y, PieceType::I);

    overflow.clear_rows(overflow.full_rows());

    bool empty_after = true;
    for(int y = 0; y < BOARD_H; ++y) if(!overflow.row_empty(y)) empty_after = false;
    check(empty_after, "clearing four full rows leaves the board completely empty");
}

// ---------------------------------------------------------------- kicks

void test_kicks()
{
    section("SRS wall kicks");

    // The first offset must always be the in-place rotation.
    bool identity_first = true;
    for(int t = 0; t < PIECE_COUNT; ++t)
    {
        PieceType const type = static_cast<PieceType>(t);
        for(int from = 0; from < 4; ++from)
        {
            for(int d : {1, -1})
            {
                int const to = ((from + d) % 4 + 4) % 4;
                KickTable const& table = kicks(type, from, to);
                if(table[0] != sf::Vector2i{0, 0}) identity_first = false;
            }
        }
    }
    check(identity_first, "the first kick offset is always no offset");

    /*
     * A J against the left wall. In spawn orientation it cannot rotate
     * counter-clockwise in place, the rotation would push it through
     * the wall, so SRS must kick it right by one.
     */
    {
        Board board;
        Piece piece{PieceType::J, 0, -1, 10};

        // Confirm the premise: the rotation in place really is blocked.
        Piece blocked = piece;
        blocked.rotation = 3;
        bool const in_place_fails = !board.fits(blocked);

        // Now walk the kick table the way the game does.
        bool kicked = false;
        for(sf::Vector2i offset : kicks(PieceType::J, 0, 3))
        {
            Piece candidate = piece;
            candidate.rotation = 3;
            candidate.x += offset.x;
            candidate.y += offset.y;
            if(board.fits(candidate)) { kicked = true; break; }
        }

        check(in_place_fails, "a J at the left wall cannot rotate in place");
        check(kicked, "SRS finds a kick that lets it rotate anyway");
    }

    /*
     * Kick tables are published y-up; ours are y-down, so every y is
     * negated on the way in. Check the sign against the reference in
     * both directions, getting this backwards is the classic SRS bug,
     * and it only shows up against a wall or a floor.
     *
     * Reference J 0->R test 4 is (0,-2), meaning two cells DOWN in a
     * y-up world, so ours must be (0,+2). R->0 is the mirror.
     */
    {
        KickTable const& forward = kicks(PieceType::J, 0, 1);
        KickTable const& back    = kicks(PieceType::J, 1, 0);

        check(forward[3] == sf::Vector2i(0, 2),
              "J 0->R test 4 moves down two, matching the reference table");
        check(back[3] == sf::Vector2i(0, -2),
              "J R->0 test 4 moves up two, matching the reference table");
    }

    /*
     * The behaviour those signs exist for: an I lying flat on the floor,
     * rotated upright, has to be lifted clear of the floor rather than
     * pushed through it.
     */
    {
        Board board;

        // Flat I resting on the floor: its cells sit in row y+1.
        Piece flat{PieceType::I, 0, 3, BOARD_H - 2};
        check(board.fits(flat), "a flat I rests on the floor");

        Piece upright = flat;
        upright.rotation = 1;
        check(!board.fits(upright), "rotating it upright in place goes through the floor");

        bool lifted = false;
        sf::Vector2i used{0, 0};
        for(sf::Vector2i offset : kicks(PieceType::I, 0, 1))
        {
            Piece candidate = flat;
            candidate.rotation = 1;
            candidate.x += offset.x;
            candidate.y += offset.y;
            if(board.fits(candidate)) { lifted = true; used = offset; break; }
        }

        check(lifted, "regression: SRS kicks it clear of the floor");
        check(used.y < 0, "and the kick that worked moves it upward, not downward");
    }

    // The I piece has its own, wider table.
    {
        KickTable const& i_table = kicks(PieceType::I, 0, 1);
        KickTable const& j_table = kicks(PieceType::J, 0, 1);
        bool differs = false;
        for(std::size_t k = 0; k < i_table.size(); ++k)
            if(i_table[k] != j_table[k]) differs = true;
        check(differs, "I uses a different kick table from the other pieces");

        bool has_two = false;
        for(sf::Vector2i offset : i_table) if(std::abs(offset.x) == 2) has_two = true;
        check(has_two, "the I table includes two-cell horizontal kicks");
    }
}

// -------------------------------------------------------------- t-spins

void test_tspin()
{
    section("T-spin detection");

    Board board;

    // T with stem down at rotation 2. Its centre is at (4, 11), and the
    // front corners for that rotation are the two below it.
    Piece piece{PieceType::T, 2, 3, 10};

    board.set(3, 12, PieceType::I);      // front-left
    board.set(5, 12, PieceType::I);      // front-right
    board.set(3, 10, PieceType::I);      // one back corner

    check(board.detect_spin(piece, true, false) == SpinResult::FULL,
          "three corners with both front corners filled is a full T-spin");

    check(board.detect_spin(piece, false, false) == SpinResult::NONE,
          "the same shape without a rotation is not a T-spin");

    // Only one front corner: a mini, unless the last kick was used.
    Board mini_board;
    Piece mini{PieceType::T, 2, 3, 10};
    mini_board.set(3, 12, PieceType::I);   // one front corner
    mini_board.set(3, 10, PieceType::I);   // two back corners
    mini_board.set(5, 10, PieceType::I);

    check(mini_board.detect_spin(mini, true, false) == SpinResult::MINI,
          "one front corner is a mini T-spin");
    check(mini_board.detect_spin(mini, true, true) == SpinResult::FULL,
          "the last kick upgrades a mini to a full T-spin");

    // Fewer than three corners is nothing at all.
    Board bare;
    bare.set(3, 12, PieceType::I);
    check(bare.detect_spin(piece, true, false) == SpinResult::NONE,
          "two corners is not a T-spin");

    // Only the T can spin.
    Board other;
    Piece l_piece{PieceType::L, 2, 3, 10};
    other.set(3, 12, PieceType::I);
    other.set(5, 12, PieceType::I);
    other.set(3, 10, PieceType::I);
    check(other.detect_spin(l_piece, true, false) == SpinResult::NONE,
          "an L in the same position is not a T-spin");
}

// -------------------------------------------------------------- scoring

void test_scoring()
{
    section("Scoring");

    {
        GameState state;
        state.begin_run();

        ClearResult const single = state.apply(1, SpinResult::NONE, false);
        check_eq(single.points, 100, "a single at level 1 scores 100");

        state.begin_run();
        ClearResult const tetris = state.apply(4, SpinResult::NONE, false);
        check_eq(tetris.points, 800, "a tetris at level 1 scores 800");
        check(tetris.label == "TETRIS", "and is labelled TETRIS");
        check(tetris.is_difficult(), "a tetris counts as difficult");
    }

    // Back-to-back: the second consecutive tetris is worth half again.
    {
        GameState state;
        state.begin_run();

        state.apply(4, SpinResult::NONE, false);
        ClearResult const second = state.apply(4, SpinResult::NONE, false);

        check_eq(second.points, 800 + 400 + 50, "back-to-back tetris pays 1.5x plus combo");
        check(second.back_to_back, "and is flagged as back-to-back");
    }

    // A non-difficult clear breaks the chain.
    {
        GameState state;
        state.begin_run();
        state.apply(4, SpinResult::NONE, false);
        state.apply(1, SpinResult::NONE, false);
        ClearResult const after = state.apply(4, SpinResult::NONE, false);
        check(!after.back_to_back, "a single between tetrises breaks back-to-back");
    }

    // Combo counts consecutive line-clearing locks and resets on a dry one.
    {
        GameState state;
        state.begin_run();

        state.apply(1, SpinResult::NONE, false);
        state.apply(1, SpinResult::NONE, false);
        ClearResult const third = state.apply(1, SpinResult::NONE, false);
        check_eq(third.combo, 2, "three clears in a row is a two-combo");

        state.apply(0, SpinResult::NONE, false);
        ClearResult const after = state.apply(1, SpinResult::NONE, false);
        check_eq(after.combo, 0, "a lock that clears nothing resets the combo");
    }

    // T-spins pay far more than the flat clear of the same size.
    {
        GameState flat;
        flat.begin_run();
        int const flat_points = flat.apply(2, SpinResult::NONE, false).points;

        GameState spun;
        spun.begin_run();
        ClearResult const tsd = spun.apply(2, SpinResult::FULL, false);

        check(tsd.points > flat_points * 3, "a T-spin double far outscores a plain double");
        check(tsd.label == "T-SPIN DOUBLE", "and is labelled correctly");
        check_eq(spun.tspins, 1, "the T-spin is counted");
    }

    // A T-spin that clears nothing still scores, and keeps the combo dead.
    {
        GameState state;
        state.begin_run();
        ClearResult const spin = state.apply(0, SpinResult::FULL, false);
        check_eq(spin.points, 400, "a T-spin with no lines scores 400");
        check_eq(state.lines, 0, "and adds no lines");
    }

    // Levels advance every ten lines and never go backwards.
    {
        GameState state;
        state.begin_run();

        for(int i = 0; i < 5; ++i) state.apply(4, SpinResult::NONE, false);
        check_eq(state.lines, 20, "twenty lines cleared");
        check_eq(state.level, 3, "which is level 3");

        float const early = state.gravity_interval();
        state.level = 12;
        float const late = state.gravity_interval();
        check(late < early, "gravity gets faster with level");
        check(late > 0.0f && early < 1.0f, "gravity stays in a sane range");
    }

    // Perfect clears carry a large bonus and relabel the callout.
    {
        GameState state;
        state.begin_run();
        ClearResult const pc = state.apply(4, SpinResult::NONE, true);
        check(pc.points > 2000, "a perfect clear tetris pays a big bonus");
        check(pc.label == "PERFECT CLEAR", "and is labelled PERFECT CLEAR");
    }

    // Scores scale with level.
    {
        GameState state;
        state.begin_run();
        state.level = 5;
        ClearResult const scaled_result = state.apply(1, SpinResult::NONE, false);
        check_eq(scaled_result.points, 500, "a single at level 5 scores 5x");
    }
}

// ------------------------------------------------------- full-game sanity

void test_simulated_game()
{
    section("Simulated play");

    /*
     * Play a few hundred pieces with a crude but real placement search:
     * try every column and rotation, and take whichever lands lowest
     * while opening the fewest holes.
     *
     * Dropping everything into one column would top out in a dozen
     * pieces and never clear a line, which tests nothing. This actually
     * fills and clears rows, so it exercises lock, clear and the
     * downward shift together, hundreds of times.
     */
    Random::seed(7);

    Board board;
    PieceBag bag;

    int locked = 0;
    int cleared_total = 0;

    /*
     * Standard Tetris-heuristic weights: keep the stack low, prefer
     * clears, punish covered holes and a jagged surface.
     */
    auto evaluate = [](const Board& b, int rows_made)
    {
        std::array<int, BOARD_W> heights{};
        heights.fill(0);

        int holes = 0;

        for(int x = 0; x < BOARD_W; ++x)
        {
            bool covered = false;
            for(int y = 0; y < BOARD_H; ++y)
            {
                if(b.at(x, y) != PieceType::NONE)
                {
                    if(!covered) heights[static_cast<std::size_t>(x)] = BOARD_H - y;
                    covered = true;
                }
                else if(covered) ++holes;
            }
        }

        long aggregate = 0;
        for(int h : heights) aggregate += h;

        long bumpiness = 0;
        for(int x = 0; x + 1 < BOARD_W; ++x)
        {
            bumpiness += std::abs(heights[static_cast<std::size_t>(x)] -
                                  heights[static_cast<std::size_t>(x + 1)]);
        }

        return 510066L * aggregate
             - 760666L * static_cast<long>(rows_made)
             + 356630L * static_cast<long>(holes)
             + 184483L * bumpiness;
    };

    for(int i = 0; i < 400; ++i)
    {
        PieceType const type = bag.next();

        Piece spawn_piece{type, 0, spawn_x(type), 0};
        if(!board.fits(spawn_piece)) break;        // topped out, which is fine

        Piece  best{};
        long   best_cost = std::numeric_limits<long>::max();
        bool   found = false;

        for(int rotation = 0; rotation < 4; ++rotation)
        {
            for(int x = -3; x < BOARD_W + 1; ++x)
            {
                Piece candidate{type, rotation, x, 0};
                if(!board.fits(candidate)) continue;

                candidate.y += board.drop_distance(candidate);

                Board probe = board;
                probe.lock(candidate);

                std::vector<int> const made = probe.full_rows();
                probe.clear_rows(made);

                long const cost = evaluate(probe, static_cast<int>(made.size()));

                if(cost < best_cost) { best_cost = cost; best = candidate; found = true; }
            }
        }

        if(!found) break;

        board.lock(best);
        ++locked;

        std::vector<int> const rows = board.full_rows();
        if(!rows.empty())
        {
            cleared_total += static_cast<int>(rows.size());
            board.clear_rows(rows);
        }
    }

    check(locked > 100, "hundreds of pieces place and lock without wedging");
    check(cleared_total > 20, "and the run actually clears lines");

    // Every occupied cell must be inside the grid and be a real piece.
    bool grid_sane = true;
    for(int y = 0; y < BOARD_H; ++y)
    {
        for(int x = 0; x < BOARD_W; ++x)
        {
            PieceType const type = board.at(x, y);
            if(type == PieceType::NONE) continue;
            if(static_cast<int>(type) < 0 || static_cast<int>(type) >= PIECE_COUNT)
                grid_sane = false;
        }
    }
    check(grid_sane, "the grid holds only valid pieces after 400 locks");

    // No complete row may survive a clear pass.
    check(board.full_rows().empty(), "no completed row is left behind");

    std::printf("        (%d pieces locked, %d rows cleared)\n", locked, cleared_total);
}

int main()
{
    std::printf("\n\033[1mTetris rule tests\033[0m\n");

    test_shapes();
    test_bag();
    test_board();
    test_kicks();
    test_tspin();
    test_scoring();
    test_simulated_game();

    std::printf("\n%d checks, \033[%sm%d failures\033[0m\n\n",
                g_checks, g_failures == 0 ? "32" : "31", g_failures);

    return g_failures == 0 ? 0 : 1;
}
