/**
 * Tetromino.cpp  -  Cascade
 */

#include "Tetromino.hpp"

namespace tetris
{

namespace
{
    /*
     * Spawn shapes, y-down. Rotations are generated from these rather
     * than typed out, so there is one source of truth per piece.
     */
    Cells spawn_shape(PieceType type)
    {
        switch(type)
        {
            case PieceType::I: return {{{0,1},{1,1},{2,1},{3,1}}};
            case PieceType::O: return {{{0,0},{1,0},{0,1},{1,1}}};
            case PieceType::T: return {{{1,0},{0,1},{1,1},{2,1}}};
            case PieceType::S: return {{{1,0},{2,0},{0,1},{1,1}}};
            case PieceType::Z: return {{{0,0},{1,0},{1,1},{2,1}}};
            case PieceType::J: return {{{0,0},{0,1},{1,1},{2,1}}};
            case PieceType::L: return {{{2,0},{0,1},{1,1},{2,1}}};
            default:           return {{{0,0},{0,0},{0,0},{0,0}}};
        }
    }

    /*
     * Clockwise quarter turn inside an NxN box, y-down:
     *     (x, y) -> (N - 1 - y, x)
     */
    Cells rotate_cw(const Cells& cells, int n)
    {
        Cells out{};
        for(std::size_t i = 0; i < cells.size(); ++i)
        {
            out[i] = { n - 1 - cells[i].y, cells[i].x };
        }
        return out;
    }

    /*
     * All four rotations of all seven pieces, built once at first use.
     */
    struct RotationTable
    {
        std::array<std::array<Cells, 4>, PIECE_COUNT> data{};

        RotationTable()
        {
            for(int t = 0; t < PIECE_COUNT; ++t)
            {
                PieceType const type = static_cast<PieceType>(t);
                int const n = box_size(type);

                data[static_cast<std::size_t>(t)][0] = spawn_shape(type);

                for(int r = 1; r < 4; ++r)
                {
                    data[static_cast<std::size_t>(t)][static_cast<std::size_t>(r)] =
                        rotate_cw(data[static_cast<std::size_t>(t)]
                                      [static_cast<std::size_t>(r - 1)], n);
                }
            }
        }
    };

    const RotationTable& rotations()
    {
        static const RotationTable table;
        return table;
    }

    /*
     * SRS kicks. Published tables are y-up; every y here is already
     * negated for our y-down board.
     */
    KickTable make(std::initializer_list<std::pair<int, int>> offsets)
    {
        KickTable table{};
        std::size_t i = 0;
        for(auto [x, y] : offsets)
        {
            if(i >= table.size()) break;
            table[i++] = sf::Vector2i{x, -y};   // negate for y-down
        }
        return table;
    }

    struct KickTables
    {
        // Indexed [from * 4 + to]; only the eight adjacent pairs are used.
        std::array<KickTable, 16> jlstz{};
        std::array<KickTable, 16> i_piece{};

        KickTables()
        {
            auto set = [](std::array<KickTable, 16>& target, int from, int to,
                          KickTable value)
            {
                target[static_cast<std::size_t>(from * 4 + to)] = value;
            };

            set(jlstz, 0, 1, make({{0,0}, {-1,0}, {-1, 1}, {0,-2}, {-1,-2}}));
            set(jlstz, 1, 0, make({{0,0}, { 1,0}, { 1,-1}, {0, 2}, { 1, 2}}));
            set(jlstz, 1, 2, make({{0,0}, { 1,0}, { 1,-1}, {0, 2}, { 1, 2}}));
            set(jlstz, 2, 1, make({{0,0}, {-1,0}, {-1, 1}, {0,-2}, {-1,-2}}));
            set(jlstz, 2, 3, make({{0,0}, { 1,0}, { 1, 1}, {0,-2}, { 1,-2}}));
            set(jlstz, 3, 2, make({{0,0}, {-1,0}, {-1,-1}, {0, 2}, {-1, 2}}));
            set(jlstz, 3, 0, make({{0,0}, {-1,0}, {-1,-1}, {0, 2}, {-1, 2}}));
            set(jlstz, 0, 3, make({{0,0}, { 1,0}, { 1, 1}, {0,-2}, { 1,-2}}));

            set(i_piece, 0, 1, make({{0,0}, {-2,0}, { 1,0}, {-2,-1}, { 1, 2}}));
            set(i_piece, 1, 0, make({{0,0}, { 2,0}, {-1,0}, { 2, 1}, {-1,-2}}));
            set(i_piece, 1, 2, make({{0,0}, {-1,0}, { 2,0}, {-1, 2}, { 2,-1}}));
            set(i_piece, 2, 1, make({{0,0}, { 1,0}, {-2,0}, { 1,-2}, {-2, 1}}));
            set(i_piece, 2, 3, make({{0,0}, { 2,0}, {-1,0}, { 2, 1}, {-1,-2}}));
            set(i_piece, 3, 2, make({{0,0}, {-2,0}, { 1,0}, {-2,-1}, { 1, 2}}));
            set(i_piece, 3, 0, make({{0,0}, { 1,0}, {-2,0}, { 1,-2}, {-2, 1}}));
            set(i_piece, 0, 3, make({{0,0}, {-1,0}, { 2,0}, {-1, 2}, { 2,-1}}));
        }
    };

    const KickTables& kick_tables()
    {
        static const KickTables tables;
        return tables;
    }

    const KickTable& no_kick()
    {
        static const KickTable table = make({{0,0}, {0,0}, {0,0}, {0,0}, {0,0}});
        return table;
    }
}

int box_size(PieceType type)
{
    if(type == PieceType::I) return 4;
    if(type == PieceType::O) return 2;
    return 3;
}

const Cells& piece_cells(PieceType type, int rotation)
{
    static const Cells empty{{{0,0},{0,0},{0,0},{0,0}}};

    int const t = static_cast<int>(type);
    if(t < 0 || t >= PIECE_COUNT) return empty;

    // Rotation wraps, and stays non-negative for counter-clockwise.
    int const r = ((rotation % 4) + 4) % 4;

    return rotations().data[static_cast<std::size_t>(t)][static_cast<std::size_t>(r)];
}

sf::Color piece_color(PieceType type)
{
    switch(type)
    {
        case PieceType::I: return AQUA;
        case PieceType::O: return YELLOW;
        case PieceType::T: return PURPLE;
        case PieceType::S: return GREEN;
        case PieceType::Z: return RED;
        case PieceType::J: return BLUE;
        case PieceType::L: return ORANGE;
        default:           return BG2;
    }
}

const char* piece_name(PieceType type)
{
    switch(type)
    {
        case PieceType::I: return "I";
        case PieceType::O: return "O";
        case PieceType::T: return "T";
        case PieceType::S: return "S";
        case PieceType::Z: return "Z";
        case PieceType::J: return "J";
        case PieceType::L: return "L";
        default:           return "-";
    }
}

int spawn_x(PieceType type)
{
    // O is a 2-wide box, so it needs a different left edge to land in
    // the same middle columns as everything else.
    return (type == PieceType::O) ? 4 : 3;
}

const KickTable& kicks(PieceType type, int from_rotation, int to_rotation)
{
    // O has rotational symmetry; there is nothing to kick around.
    if(type == PieceType::O) return no_kick();

    int const from = ((from_rotation % 4) + 4) % 4;
    int const to   = ((to_rotation   % 4) + 4) % 4;

    auto const& table = (type == PieceType::I) ? kick_tables().i_piece
                                               : kick_tables().jlstz;

    return table[static_cast<std::size_t>(from * 4 + to)];
}

// -------------------------------------------------------------- PieceBag

PieceBag::PieceBag()
{
    refill();
}

void PieceBag::refill()
{
    std::vector<PieceType> fresh;
    fresh.reserve(PIECE_COUNT);

    for(int i = 0; i < PIECE_COUNT; ++i)
        fresh.push_back(static_cast<PieceType>(i));

    std::shuffle(fresh.begin(), fresh.end(), Random::generator());

    // Prepend, so what is already queued is dealt first.
    m_bag.insert(m_bag.begin(), fresh.begin(), fresh.end());
}

PieceType PieceBag::next()
{
    if(m_bag.empty()) refill();

    PieceType const piece = m_bag.back();
    m_bag.pop_back();

    if(m_bag.size() < 7) refill();

    return piece;
}

void PieceBag::reset()
{
    m_bag.clear();
    refill();
}

}
