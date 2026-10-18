/**
 * PlayScene.cpp  -  Tetris
 */

#include "PlayScene.hpp"

#include <cstdio>

namespace tetris
{

namespace
{
    const std::string FONT_PATH = "assets/fonts/default.ttf";

    constexpr float VIEW_W = static_cast<float>(WINDOW_WIDTH);
    constexpr float VIEW_H = static_cast<float>(WINDOW_HEIGHT);

    constexpr float CELL = 22.0f;

    constexpr float BOARD_PX_W = CELL * BOARD_W;
    constexpr float BOARD_PX_H = CELL * VISIBLE_H;

    constexpr float BOARD_X = (VIEW_W - BOARD_PX_W) / 2.0f;
    constexpr float BOARD_Y = (VIEW_H - BOARD_PX_H) / 2.0f;

    constexpr float PANEL_W = 108.0f;
    constexpr float GAP     = 22.0f;

    constexpr float HOLD_X  = BOARD_X - PANEL_W - GAP;
    constexpr float HOLD_Y  = BOARD_Y;
    constexpr float HOLD_H  = 74.0f;

    constexpr float STATS_X = HOLD_X;
    constexpr float STATS_Y = HOLD_Y + HOLD_H + GAP;

    constexpr float NEXT_X  = BOARD_X + BOARD_PX_W + GAP;
    constexpr float NEXT_Y  = BOARD_Y;
    constexpr float NEXT_H  = 274.0f;

    // Input feel. DAS is the pause before a held direction repeats; ARR
    // is the repeat interval once it does.
    constexpr float DAS         = 0.145f;
    constexpr float ARR         = 0.032f;
    constexpr float SOFT_DROP   = 0.022f;

    constexpr float LOCK_DELAY  = 0.5f;
    constexpr int   MAX_RESETS  = 15;
    constexpr float CLEAR_TIME  = 0.34f;
    constexpr float SPAWN_DELAY = 0.06f;

    constexpr int PREVIEW_COUNT = 5;
}

PlayScene::PlayScene()
    : m_camera({VIEW_W, VIEW_H}, {VIEW_W / 2.0f, VIEW_H / 2.0f}),
      m_ambient({VIEW_W, VIEW_H}, 90)
{
    // Small and quick. A falling-block game wants a tap, not a jolt --
    // anything larger makes the grid hard to read mid-drop.
    m_camera.set_max_shake_offset(7.0f);
    m_camera.set_trauma_decay(3.4f);
    m_camera.set_shake_frequency(34.0f);

    m_font = Assets::font(FONT_PATH);

    m_board_glow.set_transition(TransitionFunction::EaseOutQuad);
    m_board_glow.set_duration(0.55f);
    m_board_glow.set_instant(0.0f);

    m_hold_flash.set_transition(TransitionFunction::EaseOutQuad);
    m_hold_flash.set_duration(0.35f);
    m_hold_flash.set_instant(0.0f);

    m_piece_fade.set_transition(TransitionFunction::EaseOutQuad);
    m_piece_fade.set_duration(0.09f);
    m_piece_fade.set_instant(1.0f);

    build_particles();
}

void PlayScene::build_particles()
{
    // Dust puffed sideways where a piece meets the stack.
    ParticleSystem dust;
    dust.set_lifespan(sf::seconds(0.42f));
    dust.set_drag(0.03f);
    dust.set_fade(true);
    dust.set_gravity(sf::Vector2f{0.0f, 120.0f});
    dust.set_gravity_enabled(true);
    m_particles.add_particle_system("dust", std::move(dust));

    // Vertical streaks left behind a hard drop.
    ParticleSystem trail;
    trail.set_lifespan(sf::seconds(0.3f));
    trail.set_drag(0.01f);
    trail.set_fade(true);
    m_particles.add_particle_system("trail", std::move(trail));

    // The line clear itself: bright, fast, thrown outward along the row.
    ParticleSystem burst;
    burst.set_lifespan(sf::seconds(0.85f));
    burst.set_drag(0.18f);
    burst.set_fade(true);
    burst.set_gravity(sf::Vector2f{0.0f, 240.0f});
    burst.set_gravity_enabled(true);
    m_particles.add_particle_system("burst", std::move(burst));

    // Level up: a slow rising shimmer.
    ParticleSystem rise;
    rise.set_lifespan(sf::seconds(1.6f));
    rise.set_drag(0.6f);
    rise.set_fade(true);
    rise.set_gravity(sf::Vector2f{0.0f, -30.0f});
    rise.set_gravity_enabled(true);
    m_particles.add_particle_system("rise", std::move(rise));
}

std::shared_ptr<ParticleSystem> PlayScene::system(const std::string& key)
{
    return m_particles.get_particle_system(key);
}

sf::Vector2f PlayScene::board_origin() const
{
    return {BOARD_X, BOARD_Y};
}

sf::Vector2f PlayScene::cell_position(int x, int y) const
{
    // Board row 0 is the first hidden row, so the visible area starts
    // HIDDEN_ROWS further down.
    return { BOARD_X + static_cast<float>(x) * CELL,
             BOARD_Y + static_cast<float>(y - HIDDEN_ROWS) * CELL };
}

// ---------------------------------------------------------------- setup

void PlayScene::on_enter()
{
    GameTime::resume();
    game_state().begin_run();

    m_board.clear();
    m_bag.reset();
    m_queue.clear();
    m_popups.clear();
    m_clearing_rows.clear();

    m_hold       = PieceType::NONE;
    m_hold_used  = false;
    m_phase      = Phase::PLAYING;

    m_gravity_timer = 0.0f;
    m_lock_timer    = 0.0f;
    m_lock_resets   = 0;
    m_clear_timer   = 0.0f;
    m_topout_timer  = 0.0f;
    m_spawn_delay   = 0.0f;

    m_move_dir      = 0;
    m_das_timer     = 0.0f;
    m_das_charged   = false;
    m_soft_dropping = false;

    m_danger     = 0.0f;
    m_energy     = 0.0f;
    m_last_level = 1;

    m_board_glow.set_instant(0.0f);
    m_hold_flash.set_instant(0.0f);

    m_camera.clear_shake();
    m_camera.set_center({VIEW_W / 2.0f, VIEW_H / 2.0f}, true);

    refill_queue();
    spawn_next();
}

void PlayScene::on_exit()
{
    GameTime::resume();
}

void PlayScene::refill_queue()
{
    while(static_cast<int>(m_queue.size()) < PREVIEW_COUNT + 1)
        m_queue.push_back(m_bag.next());
}

void PlayScene::spawn_next()
{
    refill_queue();

    PieceType const type = m_queue.front();
    m_queue.erase(m_queue.begin());

    refill_queue();
    spawn(type);
}

void PlayScene::spawn(PieceType type)
{
    m_piece.type     = type;
    m_piece.rotation = 0;
    m_piece.x        = spawn_x(type);
    m_piece.y        = 0;

    m_gravity_timer        = 0.0f;
    m_lock_timer           = 0.0f;
    m_lock_resets          = 0;
    m_last_move_rotation   = false;
    m_last_rotation_kicked = false;

    m_piece_fade.set_instant(0.35f);
    m_piece_fade = 1.0f;

    /*
     * Block out: the piece cannot be placed even in the spawn buffer.
     * Guideline also tops out on a lock entirely above the visible
     * field, which lock_piece() handles separately.
     */
    if(!m_board.fits(m_piece))
    {
        // Nudge up once, standard behaviour is to try one row higher
        // before declaring the game over.
        m_piece.y -= 1;
        if(!m_board.fits(m_piece))
        {
            top_out();
            return;
        }
    }
}

// ---------------------------------------------------------- piece moves

void PlayScene::reset_lock_timer(bool moved)
{
    // Move resets let you slide and spin a piece on the floor, but only
    // a bounded number of times, so you cannot stall forever.
    if(moved && m_lock_resets < MAX_RESETS)
    {
        m_lock_timer = 0.0f;
        ++m_lock_resets;
    }
}

bool PlayScene::try_move(int dx, int dy)
{
    if(m_phase != Phase::PLAYING || !m_piece.valid()) return false;

    Piece candidate = m_piece;
    candidate.x += dx;
    candidate.y += dy;

    if(!m_board.fits(candidate)) return false;

    m_piece = candidate;
    m_last_move_rotation = false;

    // Only a horizontal move or a rotation resets the lock timer;
    // gravity pulling the piece down should not.
    if(dx != 0) reset_lock_timer(true);

    return true;
}

bool PlayScene::try_rotate(int direction)
{
    if(m_phase != Phase::PLAYING || !m_piece.valid()) return false;
    if(m_piece.type == PieceType::O)
    {
        // Nothing to do, but report success so it still feels responsive.
        return true;
    }

    int const from = ((m_piece.rotation % 4) + 4) % 4;
    int const to   = ((from + direction) % 4 + 4) % 4;

    KickTable const& table = kicks(m_piece.type, from, to);

    for(std::size_t attempt = 0; attempt < table.size(); ++attempt)
    {
        Piece candidate = m_piece;
        candidate.rotation = to;
        candidate.x += table[attempt].x;
        candidate.y += table[attempt].y;

        if(!m_board.fits(candidate)) continue;

        m_piece = candidate;
        m_last_move_rotation   = true;
        m_last_rotation_kicked = (attempt == table.size() - 1);

        reset_lock_timer(true);
        return true;
    }

    return false;
}

void PlayScene::hard_drop()
{
    if(m_phase != Phase::PLAYING || !m_piece.valid()) return;

    int const distance = m_board.drop_distance(m_piece);

    if(distance > 0)
    {
        emit_drop_trail(m_piece, distance);
        m_piece.y += distance;
        m_last_move_rotation = false;

        game_state().score += distance * 2;
    }

    // Scale the jolt with how far it fell, so a slam reads harder than
    // a tap-down.
    m_camera.shake(0.14f + 0.012f * static_cast<float>(distance));

    lock_piece();
}

void PlayScene::hold_piece()
{
    if(m_phase != Phase::PLAYING || m_hold_used || !m_piece.valid()) return;

    PieceType const outgoing = m_piece.type;

    m_hold_flash.set_instant(1.0f);
    m_hold_flash = 0.0f;

    if(m_hold == PieceType::NONE)
    {
        m_hold = outgoing;
        spawn_next();
    }
    else
    {
        PieceType const incoming = m_hold;
        m_hold = outgoing;
        spawn(incoming);
    }

    // One hold per piece; using it again has to wait for the next lock.
    m_hold_used = true;
}

// --------------------------------------------------------------- locking

void PlayScene::lock_piece()
{
    if(!m_piece.valid()) return;

    SpinResult const spin = m_board.detect_spin(m_piece, m_last_move_rotation,
                                                m_last_rotation_kicked);

    /*
     * Lock out: if every cell of the locked piece is above the visible
     * field, the stack has grown out of the board.
     */
    bool entirely_hidden = true;
    for(sf::Vector2i cell : m_piece.board_cells())
    {
        if(cell.y >= HIDDEN_ROWS) { entirely_hidden = false; break; }
    }

    m_board.lock(m_piece);
    emit_lock_dust(m_piece);

    m_camera.shake(0.06f);

    std::vector<int> const rows = m_board.full_rows();

    // A perfect clear leaves nothing behind at all.
    bool perfect = false;
    if(!rows.empty())
    {
        Board probe = m_board;
        probe.clear_rows(rows);
        perfect = true;
        for(int y = 0; y < BOARD_H && perfect; ++y)
        {
            if(!probe.row_empty(y)) perfect = false;
        }
    }

    ClearResult const result = game_state().apply(static_cast<int>(rows.size()),
                                                  spin, perfect);
    announce(result);

    m_hold_used = false;
    m_piece.type = PieceType::NONE;

    if(entirely_hidden)
    {
        top_out();
        return;
    }

    if(!rows.empty())
    {
        begin_clear(rows, result);
    }
    else
    {
        m_spawn_delay = SPAWN_DELAY;
    }
}

void PlayScene::begin_clear(const std::vector<int>& rows, const ClearResult& result)
{
    m_phase         = Phase::CLEARING;
    m_clearing_rows = rows;
    m_clear_timer   = 0.0f;

    // The board lights up, harder for a bigger clear.
    float const intensity = std::min(1.0f, 0.35f + 0.2f * static_cast<float>(rows.size()));
    m_board_glow.set_instant(intensity);
    m_board_glow = 0.0f;

    m_camera.shake(0.16f + 0.1f * static_cast<float>(rows.size()));
    if(result.is_difficult()) m_camera.shake(0.2f);

    for(int row : rows)
    {
        // Burst carries the colour of what was actually in that row.
        sf::Color colour = FG2;
        for(int x = 0; x < BOARD_W; ++x)
        {
            PieceType const type = m_board.at(x, row);
            if(type != PieceType::NONE) { colour = piece_color(type); break; }
        }
        emit_clear_burst(row, colour);
    }
}

void PlayScene::finish_clear()
{
    m_board.clear_rows(m_clearing_rows);
    m_clearing_rows.clear();

    m_phase       = Phase::PLAYING;
    m_spawn_delay = SPAWN_DELAY;

    GameState& state = game_state();
    if(state.level != m_last_level)
    {
        m_last_level = state.level;

        // Level up: a rising shimmer up the board and a longer glow.
        m_board_glow.set_instant(0.9f);
        m_board_glow = 0.0f;

        if(auto rise = system("rise"))
        {
            for(int i = 0; i < 46; ++i)
            {
                rise->set_position({BOARD_X + Random::real(0.0f, BOARD_PX_W),
                                    BOARD_Y + BOARD_PX_H});
                rise->add_particles_varied(1, level_accent(state.level),
                                           Vector{Random::real(60.0f, 150.0f),
                                                  sf::degrees(90.0f)}, 22.0f, 20.0f);
            }
        }

        if(m_font)
        {
            char buffer[48];
            std::snprintf(buffer, sizeof(buffer), "LEVEL %d", state.level);
            m_popups.add(*m_font, buffer, {VIEW_W / 2.0f, BOARD_Y + BOARD_PX_H * 0.34f},
                         level_accent(state.level), 20, 1.5f);
        }
    }
}

void PlayScene::top_out()
{
    if(m_phase == Phase::TOPPED_OUT) return;

    m_phase        = Phase::TOPPED_OUT;
    m_topout_timer = 1.5f;

    m_camera.shake(0.7f);

    // Slow motion on the way out, so the last frame lands.
    GameTime::set_scale(0.3f);

    if(m_font)
    {
        m_popups.add(*m_font, "TOPPED OUT", {VIEW_W / 2.0f, VIEW_H / 2.0f},
                     RED, 22, 2.0f);
    }

    if(auto burst = system("burst"))
    {
        for(int i = 0; i < 70; ++i)
        {
            burst->set_position({BOARD_X + Random::real(0.0f, BOARD_PX_W),
                                 BOARD_Y + Random::real(0.0f, BOARD_PX_H)});
            burst->add_particles_varied(1, mix(RED, FG1, Random::real(0.0f, 0.6f)),
                                        Vector{Random::real(80.0f, 260.0f),
                                               Random::angle()}, 0.0f, 40.0f);
        }
    }
}

// -------------------------------------------------------------- effects

void PlayScene::emit_lock_dust(const Piece& piece)
{
    auto dust = system("dust");
    if(!dust) return;

    // Puff only where the piece is actually resting on something.
    for(sf::Vector2i cell : piece.board_cells())
    {
        if(!m_board.is_solid(cell.x, cell.y + 1)) continue;
        if(cell.y < HIDDEN_ROWS) continue;

        sf::Vector2f const at = cell_position(cell.x, cell.y);

        dust->set_position({at.x + CELL * 0.5f, at.y + CELL});
        dust->add_particles_varied(3, with_alpha(mix(piece.color(), FG1, 0.3f), 0.9f),
                                   Vector{70.0f, sf::degrees(0.0f)}, 180.0f, 26.0f);
    }
}

void PlayScene::emit_drop_trail(const Piece& piece, int distance)
{
    auto trail = system("trail");
    if(!trail) return;

    Cells const cells = piece.board_cells();

    // One streak per column of the piece, along the path it travelled.
    std::array<int, BOARD_W> lowest{};
    lowest.fill(-1000);

    for(sf::Vector2i cell : cells)
    {
        if(cell.x < 0 || cell.x >= BOARD_W) continue;
        lowest[static_cast<std::size_t>(cell.x)] =
            std::max(lowest[static_cast<std::size_t>(cell.x)], cell.y);
    }

    for(int x = 0; x < BOARD_W; ++x)
    {
        int const bottom = lowest[static_cast<std::size_t>(x)];
        if(bottom < -100) continue;

        for(int step = 0; step < distance; step += 2)
        {
            int const y = bottom + step;
            if(y < HIDDEN_ROWS) continue;

            sf::Vector2f const at = cell_position(x, y);
            trail->set_position({at.x + CELL * 0.5f, at.y + CELL * 0.5f});
            trail->add_particles(1, with_alpha(piece.color(), 0.65f),
                                 Vector{40.0f, sf::degrees(90.0f)}, 12.0f);
        }
    }
}

void PlayScene::emit_clear_burst(int row, sf::Color color)
{
    auto burst = system("burst");
    if(!burst) return;

    for(int x = 0; x < BOARD_W; ++x)
    {
        sf::Vector2f const at = cell_position(x, row);

        burst->set_position({at.x + CELL * 0.5f, at.y + CELL * 0.5f});

        // Thrown outward from the centre of the row, so the line reads
        // as splitting apart rather than exploding uniformly.
        float const from_centre = (static_cast<float>(x) - (BOARD_W - 1) * 0.5f);
        float const direction   = (from_centre >= 0.0f) ? 0.0f : 180.0f;

        burst->add_particles_varied(5, mix(color, FG0, 0.35f),
                                    Vector{130.0f + std::fabs(from_centre) * 26.0f,
                                           sf::degrees(direction)}, 42.0f, 60.0f);
    }
}

void PlayScene::announce(const ClearResult& result)
{
    if(!m_font || result.label.empty()) return;

    sf::Color colour = FG1;
    if(result.lines == 4)                   colour = AQUA;
    else if(result.spin != SpinResult::NONE) colour = PURPLE;
    else if(result.perfect_clear)            colour = YELLOW;
    else                                     colour = FG2;

    unsigned const size = (result.lines == 4 || result.spin != SpinResult::NONE ||
                           result.perfect_clear) ? 18u : 14u;

    sf::Vector2f const at{VIEW_W / 2.0f, BOARD_Y + BOARD_PX_H * 0.42f};

    m_popups.add(*m_font, result.label, at, colour, size, 1.35f);

    if(result.back_to_back)
    {
        m_popups.add(*m_font, "BACK TO BACK", {at.x, at.y + 24.0f},
                     mix(colour, FG0, 0.4f), 12, 1.2f);
    }

    if(result.combo > 0)
    {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%d COMBO", result.combo + 1);
        m_popups.add(*m_font, buffer, {at.x, at.y + (result.back_to_back ? 42.0f : 24.0f)},
                     ORANGE, 13, 1.2f);
    }

    // The board's glow tracks how well you are doing.
    m_energy = std::min(1.0f, m_energy + 0.25f + 0.1f * static_cast<float>(result.lines));
}

// ---------------------------------------------------------------- input

void PlayScene::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    (void)mouse_pos;

    if(const auto* pressed = event.getIf<sf::Event::KeyPressed>())
    {
        switch(pressed->code)
        {
            case sf::Keyboard::Key::Escape:
                EventBus::publish(Event(EventType::SWITCH_TO_MAIN_MENU));
                return;

            case sf::Keyboard::Key::P:
                if(m_phase != Phase::TOPPED_OUT)
                    GameTime::is_paused() ? GameTime::resume() : GameTime::pause();
                return;

            default:
                break;
        }

        if(GameTime::is_paused() || m_phase != Phase::PLAYING) return;

        switch(pressed->code)
        {
            case sf::Keyboard::Key::Left:
            case sf::Keyboard::Key::A:
                m_move_dir    = -1;
                m_das_timer   = 0.0f;
                m_das_charged = false;
                try_move(-1, 0);
                break;

            case sf::Keyboard::Key::Right:
            case sf::Keyboard::Key::D:
                m_move_dir    = 1;
                m_das_timer   = 0.0f;
                m_das_charged = false;
                try_move(1, 0);
                break;

            case sf::Keyboard::Key::Down:
            case sf::Keyboard::Key::S:
                m_soft_dropping = true;
                break;

            case sf::Keyboard::Key::Up:
            case sf::Keyboard::Key::X:
                try_rotate(1);
                break;

            case sf::Keyboard::Key::Z:
            case sf::Keyboard::Key::LControl:
                try_rotate(-1);
                break;

            case sf::Keyboard::Key::Space:
                hard_drop();
                break;

            case sf::Keyboard::Key::C:
            case sf::Keyboard::Key::LShift:
                hold_piece();
                break;

            default:
                break;
        }
    }
    else if(const auto* released = event.getIf<sf::Event::KeyReleased>())
    {
        switch(released->code)
        {
            case sf::Keyboard::Key::Left:
            case sf::Keyboard::Key::A:
                if(m_move_dir == -1) m_move_dir = 0;
                break;

            case sf::Keyboard::Key::Right:
            case sf::Keyboard::Key::D:
                if(m_move_dir == 1) m_move_dir = 0;
                break;

            case sf::Keyboard::Key::Down:
            case sf::Keyboard::Key::S:
                m_soft_dropping = false;
                break;

            default:
                break;
        }
    }
}

// --------------------------------------------------------------- update

void PlayScene::update(sf::Time delta_time)
{
    float const dt = delta_time.asSeconds();
    if(dt <= 0.0f) return;          // paused

    GameState& state = game_state();

    m_particles.update(delta_time);
    m_popups.update(delta_time);

    // Energy is what drives the ambient field and the board's rim glow;
    // it builds as you score and bleeds away when you stop.
    m_energy = std::max(0.0f, m_energy - dt * 0.28f);

    // Danger ramps up as the stack approaches the ceiling.
    int const highest = m_board.highest_filled_row();
    float const headroom = static_cast<float>(highest - HIDDEN_ROWS) /
                           static_cast<float>(VISIBLE_H);
    float const target_danger = std::clamp(1.0f - headroom * 2.2f, 0.0f, 1.0f);
    m_danger += (target_danger - m_danger) * std::min(1.0f, dt * 3.0f);

    m_ambient.update(delta_time, std::max(m_energy, m_danger * 0.4f));

    switch(m_phase)
    {
        case Phase::CLEARING:
        {
            m_clear_timer += dt;
            if(m_clear_timer >= CLEAR_TIME) finish_clear();
            break;
        }

        case Phase::TOPPED_OUT:
        {
            m_topout_timer -= dt;
            if(m_topout_timer <= 0.0f)
            {
                state.finish_run();
                GameTime::resume();
                EventBus::publish(Event(EventType::SWITCH_TO_GAME_OVER));
            }
            break;
        }

        case Phase::PLAYING:
        {
            if(m_spawn_delay > 0.0f)
            {
                m_spawn_delay -= dt;
                if(m_spawn_delay <= 0.0f) spawn_next();
                break;
            }

            if(!m_piece.valid()) break;

            // --- horizontal auto-repeat ---------------------------------
            if(m_move_dir != 0)
            {
                m_das_timer += dt;

                if(!m_das_charged)
                {
                    if(m_das_timer >= DAS)
                    {
                        m_das_charged = true;
                        m_das_timer   = 0.0f;
                        try_move(m_move_dir, 0);
                    }
                }
                else
                {
                    while(m_das_timer >= ARR)
                    {
                        m_das_timer -= ARR;
                        if(!try_move(m_move_dir, 0)) break;
                    }
                }
            }

            // --- gravity ------------------------------------------------
            float const interval = m_soft_dropping
                                 ? std::min(SOFT_DROP, state.gravity_interval())
                                 : state.gravity_interval();

            m_gravity_timer += dt;

            while(m_gravity_timer >= interval)
            {
                m_gravity_timer -= interval;

                if(try_move(0, 1))
                {
                    if(m_soft_dropping) state.score += 1;
                }
                else
                {
                    break;      // resting; the lock timer takes over
                }
            }

            // --- lock delay ---------------------------------------------
            bool const resting = (m_board.drop_distance(m_piece) == 0);

            if(resting)
            {
                m_lock_timer += dt;
                if(m_lock_timer >= LOCK_DELAY) lock_piece();
            }
            else
            {
                m_lock_timer = 0.0f;
            }

            break;
        }
    }

    m_camera.update(delta_time);
}

void PlayScene::update_realtime(sf::Time delta_time)
{
    // While paused the board should sit still, but the ambient field and
    // the pause banner keep breathing so the screen is not dead.
    if(GameTime::is_paused())
    {
        m_ambient.update(delta_time, 0.15f);
        m_camera.update(delta_time);
    }
}

// --------------------------------------------------------------- drawing

void PlayScene::draw_chrome()
{
    m_chrome.clear();

    float const glow   = m_board_glow.get_value();
    float const accent = std::clamp(m_energy * 0.6f + glow, 0.0f, 1.0f);

    sf::Color const rim = mix(BG3, level_accent(game_state().level), accent);

    // Playfield well.
    push_quad(m_chrome, {BOARD_X, BOARD_Y}, {BOARD_PX_W, BOARD_PX_H}, BG0_HARD);

    // Column and row guides, very faint.
    for(int x = 1; x < BOARD_W; ++x)
    {
        push_quad(m_chrome, {BOARD_X + static_cast<float>(x) * CELL, BOARD_Y},
                  {1.0f, BOARD_PX_H}, with_alpha(BG1, 0.5f));
    }
    for(int y = 1; y < VISIBLE_H; ++y)
    {
        push_quad(m_chrome, {BOARD_X, BOARD_Y + static_cast<float>(y) * CELL},
                  {BOARD_PX_W, 1.0f}, with_alpha(BG1, 0.32f));
    }

    // Danger wash creeping down from the ceiling.
    if(m_danger > 0.01f)
    {
        float const height = BOARD_PX_H * 0.3f;
        push_quad(m_chrome, {BOARD_X, BOARD_Y}, {BOARD_PX_W, height},
                  with_alpha(RED_DIM, m_danger * 0.16f));
    }

    push_outline(m_chrome, {BOARD_X - 2.0f, BOARD_Y - 2.0f},
                 {BOARD_PX_W + 4.0f, BOARD_PX_H + 4.0f}, 2.0f, rim);

    // A second, softer rim outside the first when energy is high.
    if(accent > 0.05f)
    {
        push_outline(m_chrome, {BOARD_X - 5.0f, BOARD_Y - 5.0f},
                     {BOARD_PX_W + 10.0f, BOARD_PX_H + 10.0f}, 1.0f,
                     with_alpha(rim, accent * 0.5f));
    }

    // Side panels.
    auto panel = [this](float x, float y, float w, float h, float flash)
    {
        push_quad(m_chrome, {x, y}, {w, h}, BG0);
        push_outline(m_chrome, {x, y}, {w, h}, 1.0f,
                     mix(BG2, level_accent(game_state().level), flash));
    };

    panel(HOLD_X,  HOLD_Y,  PANEL_W, HOLD_H,  m_hold_flash.get_value());
    panel(NEXT_X,  NEXT_Y,  PANEL_W, NEXT_H,  0.0f);
    panel(STATS_X, STATS_Y, PANEL_W, 150.0f,  0.0f);
}

void PlayScene::draw_stack()
{
    m_blocks.clear();
    m_overlay.clear();

    float const clear_t = (m_phase == Phase::CLEARING)
                        ? std::clamp(m_clear_timer / CLEAR_TIME, 0.0f, 1.0f)
                        : 0.0f;

    for(int y = HIDDEN_ROWS; y < BOARD_H; ++y)
    {
        bool const clearing = std::find(m_clearing_rows.begin(), m_clearing_rows.end(), y)
                              != m_clearing_rows.end();

        for(int x = 0; x < BOARD_W; ++x)
        {
            PieceType const type = m_board.at(x, y);
            if(type == PieceType::NONE) continue;

            sf::Vector2f const at = cell_position(x, y);

            if(!clearing)
            {
                push_block(m_blocks, at, CELL, piece_color(type));
                continue;
            }

            /*
             * Clearing rows blow out to white and shrink from both ends
             * toward the centre, so the row visibly leaves rather than
             * simply disappearing on the next frame.
             */
            float const fade  = 1.0f - clear_t;
            float const shrink = clear_t * CELL * 0.5f;

            sf::Color const hot = mix(piece_color(type), FG0,
                                      std::min(1.0f, clear_t * 2.2f));

            push_block(m_overlay, {at.x + shrink * 0.5f, at.y + shrink},
                       CELL - shrink, hot, fade);
        }
    }

    // A bright bar sweeping across each clearing row.
    if(m_phase == Phase::CLEARING)
    {
        for(int row : m_clearing_rows)
        {
            sf::Vector2f const at = cell_position(0, row);
            float const alpha = (1.0f - clear_t) * 0.55f;
            push_quad(m_overlay, {BOARD_X, at.y}, {BOARD_PX_W, CELL},
                      with_alpha(FG0, alpha));
        }
    }
}

void PlayScene::draw_active_piece()
{
    if(!m_piece.valid() || m_phase != Phase::PLAYING) return;

    float const fade = m_piece_fade.get_value();

    // Ghost first, so the real piece always sits on top of it.
    Piece const ghost = m_board.ghost_of(m_piece);
    if(ghost.y != m_piece.y)
    {
        for(sf::Vector2i cell : ghost.board_cells())
        {
            if(cell.y < HIDDEN_ROWS) continue;
            push_ghost(m_blocks, cell_position(cell.x, cell.y), CELL,
                       m_piece.color(), fade);
        }
    }

    // Resting pieces brighten slightly as the lock timer runs out, which
    // is the only warning that the piece is about to commit.
    float const lock_t = std::clamp(m_lock_timer / LOCK_DELAY, 0.0f, 1.0f);
    sf::Color const colour = mix(m_piece.color(), FG0, lock_t * 0.4f);

    for(sf::Vector2i cell : m_piece.board_cells())
    {
        if(cell.y < HIDDEN_ROWS) continue;
        push_block(m_blocks, cell_position(cell.x, cell.y), CELL, colour, fade);
    }
}

void PlayScene::draw_mini_piece(sf::VertexArray& va, PieceType type,
                                sf::Vector2f centre, float cell, float alpha)
{
    if(type == PieceType::NONE) return;

    Cells const& cells = piece_cells(type, 0);

    // Centre the piece on its own bounding box rather than the rotation
    // box, so I and O do not sit visibly off-centre in the preview.
    int min_x = 4, max_x = -1, min_y = 4, max_y = -1;
    for(sf::Vector2i c : cells)
    {
        min_x = std::min(min_x, c.x); max_x = std::max(max_x, c.x);
        min_y = std::min(min_y, c.y); max_y = std::max(max_y, c.y);
    }

    float const w = static_cast<float>(max_x - min_x + 1) * cell;
    float const h = static_cast<float>(max_y - min_y + 1) * cell;

    sf::Vector2f const origin{centre.x - w / 2.0f, centre.y - h / 2.0f};

    for(sf::Vector2i c : cells)
    {
        sf::Vector2f const at{origin.x + static_cast<float>(c.x - min_x) * cell,
                              origin.y + static_cast<float>(c.y - min_y) * cell};
        push_block(va, at, cell, piece_color(type), alpha);
    }
}

void PlayScene::draw_preview_and_hold()
{
    constexpr float MINI = 13.0f;

    // Hold.
    float const hold_alpha = m_hold_used ? 0.35f : 1.0f;
    draw_mini_piece(m_blocks, m_hold,
                    {HOLD_X + PANEL_W / 2.0f, HOLD_Y + HOLD_H / 2.0f + 6.0f},
                    MINI, hold_alpha);

    // Next queue. The first is full size and full brightness; the rest
    // taper, which makes the reading order obvious at a glance.
    int const shown = std::min(PREVIEW_COUNT, static_cast<int>(m_queue.size()));

    for(int i = 0; i < shown; ++i)
    {
        float const t     = static_cast<float>(i);
        float const cell  = (i == 0) ? MINI : MINI - 2.0f;
        float const alpha = (i == 0) ? 1.0f : std::max(0.35f, 0.85f - t * 0.14f);
        float const y     = NEXT_Y + 34.0f + t * 50.0f;

        draw_mini_piece(m_blocks, m_queue[static_cast<std::size_t>(i)],
                        {NEXT_X + PANEL_W / 2.0f, y}, cell, alpha);
    }
}

void PlayScene::draw_text(sf::RenderTarget& target)
{
    if(!m_font) return;

    GameState const& state = game_state();

    auto label = [&](const std::string& text, sf::Vector2f at, sf::Color colour,
                     unsigned size, bool centred)
    {
        sf::Text item(*m_font, text, size);
        item.setFillColor(colour);

        if(centred)
        {
            sf::FloatRect const b = item.getLocalBounds();
            item.setOrigin({b.size.x / 2.0f, 0.0f});
        }

        item.setPosition(floor_vector(at));
        target.draw(item);
    };

    label("HOLD", {HOLD_X + PANEL_W / 2.0f, HOLD_Y + 5.0f}, FG4, 10, true);
    label("NEXT", {NEXT_X + PANEL_W / 2.0f, NEXT_Y + 6.0f}, FG4, 10, true);

    char buffer[64];

    float y = STATS_Y + 10.0f;
    auto stat = [&](const char* name, int value, sf::Color colour)
    {
        label(name, {STATS_X + 10.0f, y}, FG4, 10, false);
        std::snprintf(buffer, sizeof(buffer), "%d", value);
        label(buffer, {STATS_X + PANEL_W / 2.0f, y + 13.0f}, colour, 15, true);
        y += 38.0f;
    };

    stat("SCORE", state.score, FG1);
    stat("LINES", state.lines, FG2);
    stat("LEVEL", state.level, level_accent(state.level));

    if(state.combo > 0)
    {
        std::snprintf(buffer, sizeof(buffer), "COMBO %d", state.combo + 1);
        label(buffer, {STATS_X + PANEL_W / 2.0f, y}, ORANGE, 11, true);
    }
    else if(state.back_to_back)
    {
        label("B2B READY", {STATS_X + PANEL_W / 2.0f, y}, AQUA, 11, true);
    }

    if(GameTime::is_paused())
    {
        label("PAUSED", {VIEW_W / 2.0f, BOARD_Y + BOARD_PX_H / 2.0f - 10.0f},
              FG0, 20, true);
        label("P to resume    ESC to quit",
              {VIEW_W / 2.0f, BOARD_Y + BOARD_PX_H / 2.0f + 18.0f}, FG4, 10, true);
    }
}

void PlayScene::render(sf::RenderTarget& target)
{
    // Ambient field sits behind everything and does not shake, it is
    // the backdrop, not part of the board.
    target.setView(target.getDefaultView());
    m_ambient.draw(target);

    /*
     * Everything else goes through the camera's UI view, so the whole
     * playfield, well, stack, panels and text together, jolts as one
     * on a hard drop or a clear. Shaking only the blocks would look like
     * a rendering bug.
     */
    target.setView(m_camera.get_ui_view());

    draw_chrome();
    draw_stack();
    draw_active_piece();
    draw_preview_and_hold();

    target.draw(m_chrome);
    target.draw(m_blocks);
    target.draw(m_particles);
    target.draw(m_overlay);

    draw_text(target);
    m_popups.draw(target);
}

sf::Color PlayScene::get_clear_color() const
{
    // A touch of red bleeds into the background when the stack is high.
    return mix(BG0_HARD, sf::Color(0x2a, 0x18, 0x18), m_danger * 0.55f);
}

}
