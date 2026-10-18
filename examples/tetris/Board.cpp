/**
 * Board.cpp  -  Cascade
 */

#include "Board.hpp"

namespace tetris
{

Board::Board()
{
    clear();
}

void Board::clear()
{
    m_cells.fill(PieceType::NONE);
}

bool Board::in_bounds(int x, int y) const
{
    return x >= 0 && x < BOARD_W && y >= 0 && y < BOARD_H;
}

bool Board::is_solid(int x, int y) const
{
    // Walls and floor block; open sky above the board does not.
    if(x < 0 || x >= BOARD_W) return true;
    if(y >= BOARD_H)          return true;
    if(y < 0)                 return false;

    return m_cells[index(x, y)] != PieceType::NONE;
}

PieceType Board::at(int x, int y) const
{
    if(!in_bounds(x, y)) return PieceType::NONE;
    return m_cells[index(x, y)];
}

void Board::set(int x, int y, PieceType type)
{
    if(!in_bounds(x, y)) return;
    m_cells[index(x, y)] = type;
}

bool Board::fits(const Piece& piece) const
{
    if(!piece.valid()) return false;

    for(sf::Vector2i cell : piece.board_cells())
    {
        if(is_solid(cell.x, cell.y)) return false;
    }
    return true;
}

void Board::lock(const Piece& piece)
{
    for(sf::Vector2i cell : piece.board_cells())
    {
        set(cell.x, cell.y, piece.type);
    }
}

int Board::drop_distance(const Piece& piece) const
{
    int distance = 0;
    Piece probe = piece;

    // Bounded by the board height so a malformed piece cannot spin here.
    while(distance <= BOARD_H)
    {
        probe.y = piece.y + distance + 1;
        if(!fits(probe)) break;
        ++distance;
    }

    return distance;
}

Piece Board::ghost_of(const Piece& piece) const
{
    Piece ghost = piece;
    ghost.y += drop_distance(piece);
    return ghost;
}

bool Board::row_full(int y) const
{
    if(y < 0 || y >= BOARD_H) return false;

    for(int x = 0; x < BOARD_W; ++x)
    {
        if(m_cells[index(x, y)] == PieceType::NONE) return false;
    }
    return true;
}

bool Board::row_empty(int y) const
{
    if(y < 0 || y >= BOARD_H) return true;

    for(int x = 0; x < BOARD_W; ++x)
    {
        if(m_cells[index(x, y)] != PieceType::NONE) return false;
    }
    return true;
}

std::vector<int> Board::full_rows() const
{
    std::vector<int> rows;
    for(int y = 0; y < BOARD_H; ++y)
    {
        if(row_full(y)) rows.push_back(y);
    }
    return rows;
}

void Board::clear_rows(const std::vector<int>& rows)
{
    if(rows.empty()) return;

    /*
     * Walk from the bottom up, copying surviving rows downward into a
     * write cursor. One pass, and it handles non-adjacent rows without
     * any special casing.
     */
    std::array<bool, BOARD_H> doomed{};
    doomed.fill(false);
    for(int row : rows)
    {
        if(row >= 0 && row < BOARD_H) doomed[static_cast<std::size_t>(row)] = true;
    }

    int write = BOARD_H - 1;
    for(int read = BOARD_H - 1; read >= 0; --read)
    {
        if(doomed[static_cast<std::size_t>(read)]) continue;

        if(write != read)
        {
            for(int x = 0; x < BOARD_W; ++x)
                m_cells[index(x, write)] = m_cells[index(x, read)];
        }
        --write;
    }

    // Anything left at the top is now empty sky.
    for(; write >= 0; --write)
    {
        for(int x = 0; x < BOARD_W; ++x)
            m_cells[index(x, write)] = PieceType::NONE;
    }
}

SpinResult Board::detect_spin(const Piece& piece, bool last_move_was_rotation,
                              bool used_kick) const
{
    if(piece.type != PieceType::T)    return SpinResult::NONE;
    if(!last_move_was_rotation)       return SpinResult::NONE;

    // The T's centre sits at (1,1) of its 3x3 box in every rotation.
    int const cx = piece.x + 1;
    int const cy = piece.y + 1;

    // Corners in spawn orientation: the two "front" corners are the
    // ones the T's stem points between.
    struct Corner { int dx, dy; };
    static const std::array<Corner, 4> corners = {{
        {-1, -1}, {1, -1}, {1, 1}, {-1, 1}
    }};

    // Which two corners are the front pair, per rotation.
    static const std::array<std::pair<int, int>, 4> front_pairs = {{
        {0, 1},   // spawn: stem up  -> top-left, top-right
        {1, 2},   // right           -> top-right, bottom-right
        {2, 3},   // flipped         -> bottom-right, bottom-left
        {3, 0}    // left            -> bottom-left, top-left
    }};

    int filled = 0;
    std::array<bool, 4> corner_filled{};

    for(std::size_t i = 0; i < corners.size(); ++i)
    {
        bool const solid = is_solid(cx + corners[i].dx, cy + corners[i].dy);
        corner_filled[i] = solid;
        if(solid) ++filled;
    }

    if(filled < 3) return SpinResult::NONE;

    int const r = ((piece.rotation % 4) + 4) % 4;
    auto const [fa, fb] = front_pairs[static_cast<std::size_t>(r)];

    bool const both_front = corner_filled[static_cast<std::size_t>(fa)] &&
                            corner_filled[static_cast<std::size_t>(fb)];

    // Both front corners filled is a full T-spin. Only one means a mini,
    // unless the rotation needed the last, largest kick -- that kick is
    // only reachable by a genuine spin, so it upgrades to full.
    if(both_front) return SpinResult::FULL;

    return used_kick ? SpinResult::FULL : SpinResult::MINI;
}

int Board::highest_filled_row() const
{
    for(int y = 0; y < BOARD_H; ++y)
    {
        if(!row_empty(y)) return y;
    }
    return BOARD_H;
}

}
