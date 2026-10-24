/**
 * Effects.cpp  -  Mud Cats
 *
 * Everything that is only there to be seen, drawn with the framework's
 * particle systems. Each kind of thing has its own system, because each
 * wants its own look:
 *
 *   smoke    soft squares that swell and fade: gun smoke, dust, the
 *            smoke after an explosion
 *   fire     additive squares, white to yellow to red: flamethrowers,
 *            burning men, fireballs. Where they overlap they glow.
 *   sparks   additive streaks: muzzle flashes, ricochets, glass, splashes
 *   blood    thrown up into the air; where each drop comes down it stains
 *            the ground for good
 *   dirt     the same, for earth thrown up by shells and rounds
 *   rounds   every bullet in flight: a hot streak, brighter for tracers
 *   glow     big soft halos: the bloom of a muzzle blast, a tracer's
 *            core, a flare hanging over the field
 *   trails   the faint glow behind tracers
 *   rain     on the Ochre line, it never stops
 *   screen   smoke screens: big, slow and thick enough to hide a section
 *
 * The callers in Combat.cpp only say where: these functions say how.
 */

#include "Battle.hpp"

#include "Draw.hpp"
#include "Palette.hpp"

#include "../../include/core/Random.hpp"

#include <algorithm>
#include <cmath>

namespace mudcats
{

namespace
{
    float length(sf::Vector2f v) { return std::sqrt(v.x * v.x + v.y * v.y); }
    sf::Vector2f dir(float a) { return {std::cos(a), std::sin(a)}; }
    sf::Vector2f unit(sf::Vector2f v) { float const l = length(v); return l > 1e-5f ? v / l : sf::Vector2f{1.0f, 0.0f}; }

    sf::Vector2f jitter(float r) { return {Random::real(-r, r), Random::real(-r, r)}; }
    sf::Time seconds(float s) { return sf::seconds(s); }

    // A soft halo out of square particles: a few nested squares, fainter
    // outward, so it falls off in steps instead of ending in a hard box.
    void halo(ParticleSystem& fx, sf::Vector2f at, float life, sf::Color c, float alpha, float size, float end_size)
    {
        static const float scale[] = {1.0f, 0.72f, 0.46f, 0.24f};
        static const float share[] = {0.18f, 0.22f, 0.28f, 0.36f};
        for(int k = 0; k < 4; ++k)
            fx.emit(at, {}, seconds(life), with_alpha(c, alpha * share[k]), with_alpha(c, 0.0f), size * scale[k], end_size * scale[k]);
    }
}

void Battle::setup_effects()
{
    auto make = [this](const char* key, ParticleShape shape, sf::BlendMode blend, float drag, std::size_t max)
    {
        auto system = std::make_shared<ParticleSystem>();
        system->set_shape(shape);
        system->set_blend_mode(blend);
        system->set_pixel_snap(true);
        system->set_drag(drag);
        system->set_max_particles(max);
        m_fx.add_particle_system(key, system);
        return system;
    };

    m_smoke_fx  = make("smoke",  ParticleShape::SQUARE, sf::BlendAlpha, 0.3f, 5000);
    m_fire_fx   = make("fire",   ParticleShape::SQUARE, sf::BlendAdd,   0.15f, 4000);
    m_spark_fx  = make("sparks", ParticleShape::LINE,   sf::BlendAdd,   0.03f, 3000);
    m_blood_fx  = make("blood",  ParticleShape::SQUARE, sf::BlendAlpha, 0.6f, 4000);
    m_dirt_fx   = make("dirt",   ParticleShape::SQUARE, sf::BlendAlpha, 0.5f, 4000);
    m_trail_fx  = make("trails", ParticleShape::SQUARE, sf::BlendAdd,   1.0f, 3000);
    m_rain_fx   = make("rain",   ParticleShape::LINE,   sf::BlendAlpha, 1.0f, 2500);
    m_screen_fx = make("screen", ParticleShape::SQUARE, sf::BlendAlpha, 0.5f, 3000);
    m_round_fx  = make("rounds", ParticleShape::SQUARE, sf::BlendAdd,   1.0f, 16000);
    m_glow_fx   = make("glow",   ParticleShape::SQUARE, sf::BlendAdd,   0.2f, 4000);
    m_glow_fx->set_pixel_snap(false);
    m_round_fx->set_fade(true);
    m_glow_fx->set_fade(true);

    m_smoke_fx->set_fade(true);
    m_screen_fx->set_fade(false);       // colour carries the fade: thick, then gone
    m_spark_fx->set_fade(true);
    m_trail_fx->set_fade(true);

    // Blood and dirt fall, and leave a mark where they land.
    m_blood_fx->set_height_gravity(260.0f);
    m_blood_fx->set_on_land([this](const Particle& p)
    {
        sf::Vector2f const at = p.get_position();
        float const s = std::round(p.get_size());
        if(m_map.in_water(at)) return;
        draw::rect(m_stamps, std::floor(at.x), std::floor(at.y), s, s, Random::chance(0.5f) ? BLOOD : BLOOD_DK);
    });
    m_dirt_fx->set_height_gravity(260.0f);
    m_dirt_fx->set_on_land([this](const Particle& p)
    {
        sf::Vector2f const at = p.get_position();
        if(m_map.in_water(at)) return;
        sf::Color const a = m_level == LevelId::FIELDS ? MEADOW_DK : MUD_PALE;
        draw::rect(m_stamps, std::floor(at.x), std::floor(at.y), std::round(p.get_size()), 1, Random::chance(0.5f) ? a : MUD_DARK);
    });
}

void Battle::spray_blood(sf::Vector2f at, sf::Vector2f d, int count, float speed)
{
    d = unit(d);
    for(int i = 0; i < count; ++i)
    {
        sf::Vector2f const v = unit(d + jitter(0.6f)) * speed * Random::real(0.2f, 1.0f);
        float const size = Random::chance(0.3f) ? 2.0f : 1.0f;
        m_blood_fx->emit(at, v, seconds(3.0f), BLOOD_BRIGHT, BLOOD, size, size, 3.0f, Random::real(10.0f, 60.0f));
    }
    // A fine mist hangs a moment where a heavy hit landed.
    if(count >= 12)
        for(int i = 0; i < count / 6; ++i)
            m_smoke_fx->emit(at + jitter(1.5f), d * Random::real(5.0f, 25.0f), seconds(Random::real(0.25f, 0.5f)),
                             with_alpha(BLOOD_BRIGHT, 0.55f), 1.0f, 4.0f);
}

void Battle::dust(sf::Vector2f at, int count, sf::Color colour)
{
    for(int i = 0; i < count; ++i)
    {
        m_smoke_fx->emit(at, {Random::real(-12, 12), Random::real(-18, -4)}, seconds(0.5f), with_alpha(colour, 0.6f), 1.5f, 5.0f);
        m_dirt_fx->emit(at, {Random::real(-30, 30), Random::real(-30, 30)}, seconds(2.0f), colour, 1.0f, 1.0f, 1.0f, Random::real(20.0f, 50.0f));
    }
}

void Battle::muzzle(const Soldier& s)
{
    // The muzzle, unless it is through a wall: then the flash is on this side.
    sf::Vector2f tip = s.pos + dir(s.aim) * 9.0f;
    if(auto t = m_map.wall_hit(s.pos, tip)) tip = s.pos + dir(s.aim) * std::max(0.0f, *t * 9.0f - 1.0f);
    sf::Vector2f const fwd = dir(s.aim), side{-fwd.y, fwd.x};
    bool const heavy = s.has_mg() || s.gun == Gun::RIFLE || s.gun == Gun::SNIPER;
    float const big = heavy ? 1.0f : 0.65f;

    // The flash: a white-hot core, a tongue of flame thrown forward, two
    // short petals out to the sides, a burst of sparks, and a wide soft
    // halo behind it all that lights up the man and the ground.
    m_fire_fx->emit(tip, fwd * 10.0f, seconds(0.05f), FIRE_WHITE, FIRE_YELLOW, 4.0f * big, 1.0f);
    for(int k = 1; k <= 3; ++k)
        m_fire_fx->emit(tip + fwd * (2.5f * static_cast<float>(k) * big), fwd * 50.0f, seconds(0.045f), k == 1 ? FIRE_WHITE : FIRE_YELLOW,
                        with_alpha(FIRE_RED, 0.0f), (4.0f - static_cast<float>(k)) * big, 1.0f);
    for(int side_k : {-1, 1})
        m_fire_fx->emit(tip + side * (2.0f * static_cast<float>(side_k) * big), side * (30.0f * static_cast<float>(side_k)) + fwd * 15.0f, seconds(0.04f),
                        FIRE_YELLOW, with_alpha(FIRE_RED, 0.0f), 2.0f * big, 1.0f);
    halo(*m_glow_fx, tip + fwd * 2.0f, 0.07f, FIRE_YELLOW, 0.55f, 14.0f * big, 20.0f * big);
    for(int i = 0; i < (heavy ? 5 : 3); ++i)
        m_spark_fx->emit(tip, dir(s.aim + Random::real(-0.6f, 0.6f)) * Random::real(80.0f, 180.0f), seconds(Random::real(0.05f, 0.1f)), FIRE_YELLOW);

    // The smoke that hangs after it.
    m_smoke_fx->emit(tip, fwd * 14.0f + jitter(3.0f), seconds(Random::real(0.6f, 1.0f)),
                     with_alpha(SMOKE, heavy ? 0.4f : 0.28f), 2.0f, 7.0f);

    // And brass, kicked out to the right of the weapon.
    if(s.gun != Gun::PISTOL && Random::chance(0.7f))
        m_dirt_fx->emit(s.pos + fwd * 3.0f, side * Random::real(25.0f, 45.0f) - fwd * 8.0f, seconds(2.0f),
                        MON_GOLD, 1.0f, 1.0f, 4.0f, Random::real(30.0f, 50.0f));
}

void Battle::puff(sf::Vector2f at, sf::Vector2f vel, float size, float grow, float life, sf::Color colour)
{
    m_smoke_fx->emit(at, vel, seconds(life), colour, size, size + grow * life);
}

void Battle::flame_lick(sf::Vector2f at, sf::Vector2f vel, float size, float life)
{
    m_fire_fx->emit(at, vel, seconds(life), FIRE_WHITE, with_alpha(FIRE_RED, 0.0f), size, size * 2.4f);
}

void Battle::sparks(sf::Vector2f at, int count, float speed, sf::Color colour, float life)
{
    for(int i = 0; i < count; ++i)
        m_spark_fx->emit(at, dir(Random::real(0.0f, 6.283f)) * speed * Random::real(0.3f, 1.0f), seconds(life * Random::real(0.5f, 1.0f)), colour);
}

void Battle::debris(sf::Vector2f at, int count, float power)
{
    sf::Color const earth = m_level == LevelId::FIELDS ? lerp(MEADOW_DK, MUD_PALE, 0.5f) : MUD_LIGHT;
    for(int i = 0; i < count; ++i)
    {
        float const size = Random::chance(0.3f) ? 2.0f : 1.0f;
        m_dirt_fx->emit(at, dir(Random::real(0.0f, 6.283f)) * Random::real(20.0f, 120.0f) * power, seconds(3.0f),
                        earth, size, size, 2.0f, Random::real(60.0f, 180.0f));
    }
    // A column of earth thrown straight up, hanging as dust.
    for(int i = 0; i < 8; ++i)
        m_smoke_fx->emit(at + jitter(4.0f), {Random::real(-8, 8), Random::real(-40, -15)}, seconds(Random::real(0.8f, 1.4f)),
                         with_alpha(lerp(earth, MUD_DARK, 0.3f), 0.75f), 3.0f * power, 9.0f * power);
}

void Battle::update_effects(float dt)
{
    sf::View const& view = m_camera.get_view();
    sf::FloatRect const seen(view.getCenter() - view.getSize() * 0.5f - sf::Vector2f{20, 20}, view.getSize() + sf::Vector2f{40, 40});

    // --- flamethrower jets: each burning gob licks as it goes ---------------------------
    for(const auto& f : m_flames)
    {
        float const age = 1.0f - f.life / f.max_life;
        if(Random::chance(0.8f))
            flame_lick(f.pos + jitter(1.0f), f.vel * 0.3f + sf::Vector2f{0.0f, -6.0f}, 2.0f + age * 2.0f, Random::real(0.12f, 0.25f));
        if(age > 0.6f && Random::chance(0.15f))
            puff(f.pos, {Random::real(-4, 4), Random::real(-14, -6)}, 2.0f, 6.0f, Random::real(0.8f, 1.4f), with_alpha(lerp(SMOKE, INK, 0.4f), 0.4f));
    }

    // --- fires on the ground -----------------------------------------------------------------
    for(auto& fire : m_fires)
    {
        fire.spawn -= dt;
        if(fire.spawn > 0.0f) continue;
        fire.spawn = 0.05f;
        sf::Vector2f const at = fire.pos + sf::Vector2f{Random::real(-fire.radius, fire.radius), Random::real(-fire.radius, fire.radius) * 0.6f};
        flame_lick(at, {Random::real(-4, 4), Random::real(-22, -10)}, Random::real(1.5f, 2.5f), Random::real(0.3f, 0.5f));
        if(Random::chance(0.2f))
            puff(fire.pos, {Random::real(-4, 4), Random::real(-14, -6)}, 2.0f, 4.0f, 1.6f, with_alpha(SMOKE, 0.4f));
    }

    // --- smoke screens -------------------------------------------------------------------------
    for(auto& sm : m_smokes)
    {
        sm.spawn -= dt;
        if(sm.spawn > 0.0f || sm.life < 2.0f) continue;
        sm.spawn = 0.05f;
        for(int i = 0; i < 3; ++i)
        {
            float const a = Random::real(0.0f, 6.283f);
            float const r = std::sqrt(Random::real(0.0f, 1.0f)) * sm.radius;
            sf::Color const c = lerp(MON_WHITE, SMOKE, Random::real(0.3f, 0.8f));
            m_screen_fx->emit(sm.pos + dir(a) * r, sf::Vector2f{6.0f, -2.0f} + dir(a) * Random::real(2.0f, 8.0f), seconds(Random::real(2.5f, 4.0f)),
                              with_alpha(c, 0.6f), with_alpha(c, 0.0f), Random::real(6.0f, 10.0f), Random::real(14.0f, 20.0f));
        }
    }

    // --- rounds in flight -----------------------------------------------------------------
    // Every round leaves a hot streak along where it went this frame,
    // fading over the next few: a dash of light crossing the field. The
    // tracers are thicker, hotter, and carry a halo of their own.
    for(const auto& b : m_bullets)
    {
        if(!seen.contains(b.pos) && !seen.contains(b.prev)) continue;
        sf::Vector2f const step = b.pos - b.prev;
        float const len = length(step);
        if(len < 0.5f) continue;
        int const n = std::max(1, static_cast<int>(len / 2.0f));
        for(int i = 0; i < n; ++i)
        {
            float const k = (static_cast<float>(i) + Random::real(0.0f, 1.0f)) / static_cast<float>(n);
            sf::Vector2f const at = b.prev + step * k;
            if(b.tracer)
            {
                m_round_fx->emit(at, {}, seconds(0.09f), FIRE_WHITE, with_alpha(FIRE_ORANGE, 0.0f), 2.0f, 1.0f);
                if(i % 3 == 0) m_glow_fx->emit(at, {}, seconds(0.12f), with_alpha(TRACER, 0.22f), with_alpha(FIRE_RED, 0.0f), 6.0f, 3.0f);
            }
            else
            {
                sf::Color const hot = b.heavy ? sf::Color(255, 235, 190, 230) : sf::Color(255, 226, 170, 200);
                m_round_fx->emit(at, {}, seconds(0.05f), hot, sf::Color(255, 140, 60, 0), b.heavy ? 1.6f : 1.0f, 1.0f);
            }
        }
        // The head of the round, brightest.
        m_round_fx->emit(b.pos, {}, seconds(0.02f), FIRE_WHITE, FIRE_WHITE, b.tracer ? 2.0f : 1.0f, 1.0f);
        if(b.tracer) m_trail_fx->emit(b.pos, {}, seconds(0.25f), with_alpha(TRACER, 0.35f), 2.0f, 1.0f);
    }

    // --- flares hanging over the field ------------------------------------------------------
    for(const auto& f : m_flares)
    {
        sf::Vector2f const p = f.pos - sf::Vector2f{0.0f, f.height};
        sf::Color const core = f.illum ? sf::Color(240, 244, 255) : FIRE_RED;
        if(!f.fired)
        {
            // Rising: a spitting trail.
            m_fire_fx->emit(p, {Random::real(-4, 4), Random::real(4, 10)}, seconds(0.3f), FIRE_YELLOW, with_alpha(FIRE_RED, 0.0f), 2.0f, 1.0f);
            m_smoke_fx->emit(p, {Random::real(-2, 2), Random::real(2, 5)}, seconds(0.9f), with_alpha(SMOKE, 0.3f), 1.0f, 4.0f);
        }
        else
        {
            m_round_fx->emit(p, {}, seconds(0.03f), FIRE_WHITE, core, 3.0f, 2.0f);
            halo(*m_glow_fx, p, 0.05f, core, f.illum ? 0.35f : 0.3f, f.illum ? 22.0f : 16.0f, f.illum ? 26.0f : 20.0f);
            if(Random::chance(0.4f))
                m_fire_fx->emit(p, {Random::real(-6, 6), Random::real(8, 20)}, seconds(0.5f), FIRE_WHITE, with_alpha(core, 0.0f), 1.0f, 1.0f);
        }
    }

    // --- rain on the Ochre line --------------------------------------------------------------
    if(m_level == LevelId::TRENCHES)
    {
        float const per_second = 900.0f;
        int n = static_cast<int>(per_second * dt);
        if(Random::chance(per_second * dt - static_cast<float>(n))) ++n;
        sf::Vector2f const wind{-35.0f, 300.0f};
        for(int i = 0; i < n; ++i)
        {
            sf::Vector2f const at{Random::real(seen.position.x, seen.position.x + seen.size.x),
                                  Random::real(seen.position.y - 20.0f, seen.position.y + seen.size.y)};
            float const life = Random::real(0.05f, 0.12f);
            m_rain_fx->emit(at, wind * Random::real(0.9f, 1.1f), seconds(life), sf::Color(150, 160, 175, 90));
            // Where it lands, a little splash.
            if(Random::chance(0.12f))
            {
                sf::Vector2f const down = at + wind * life;
                bool const pool = m_map.in_water(down) || m_map.in_crater(down) || m_map.in_trench(down);
                m_smoke_fx->emit(down, {}, seconds(0.15f), sf::Color(170, 180, 190, pool ? 110 : 60), 1.0f, pool ? 3.0f : 1.0f);
            }
        }
    }

    m_fx.update(sf::seconds(dt));
}

}
