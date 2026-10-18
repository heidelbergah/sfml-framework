/**
 * Entities.cpp  -  Ironvale Arena
 */

#include "Entities.hpp"

namespace arena
{

bool circles_overlap(sf::Vector2f a, float ra, sf::Vector2f b, float rb)
{
    float const r = ra + rb;
    return length_squared(b - a) <= r * r;
}

void confine(sf::Vector2f& pos, sf::Vector2f& velocity, float radius)
{
    if(pos.x < radius)            { pos.x = radius;            velocity.x = 0.0f; }
    if(pos.y < radius)            { pos.y = radius;            velocity.y = 0.0f; }
    if(pos.x > ARENA_W - radius)  { pos.x = ARENA_W - radius;  velocity.x = 0.0f; }
    if(pos.y > ARENA_H - radius)  { pos.y = ARENA_H - radius;  velocity.y = 0.0f; }
}

// ------------------------------------------------------------------ Tank

Tank::Tank()
{
    // The turret eases toward the cursor rather than snapping to it,
    // which is most of what makes a turret feel like it has mass.
    turret_degrees.set_transition(TransitionFunction::EaseOutQuad);
    turret_degrees.set_duration(0.12f);
}

void Tank::update(sf::Time dt)
{
    float const seconds = dt.asSeconds();

    pos += velocity * seconds;

    // Ground friction, framerate-independent.
    velocity *= std::pow(0.02f, seconds);

    confine(pos, velocity, RADIUS);

    if(fire_cooldown > 0.0f) fire_cooldown -= seconds;
    if(invuln > 0.0f)        invuln        -= seconds;

    recoil = std::max(0.0f, recoil - seconds * 6.0f);

    heat = std::max(0.0f, heat - COOL_RATE * seconds);

    // Hysteresis: once overheated you have to cool well below the limit
    // before the gun comes back, so it cannot chatter on the boundary.
    if(overheated && heat < 35.0f) overheated = false;
}

void Tank::aim_at(sf::Vector2f world_target)
{
    sf::Vector2f const to_target = world_target - pos;
    if(length_squared(to_target) < 1.0f) return;

    float const degrees = to_polar(to_target).direction.asDegrees();

    // Unwrap so crossing the +/-180 boundary does not send the turret
    // the long way round.
    float current = turret_degrees.get_target();
    float delta   = degrees - current;
    while(delta >  180.0f) delta -= 360.0f;
    while(delta < -180.0f) delta += 360.0f;

    turret_degrees = current + delta;
}

bool Tank::can_fire() const
{
    return fire_cooldown <= 0.0f && !overheated && health > 0.0f;
}

void Tank::on_fired()
{
    fire_cooldown = FIRE_DELAY;
    recoil        = 1.0f;
    heat         += HEAT_PER_SHOT;

    if(heat >= 100.0f)
    {
        heat       = 100.0f;
        overheated = true;
    }

    // Recoil shoves the hull backwards a little.
    velocity -= aim_direction() * 42.0f;
}

void Tank::damage(float amount)
{
    if(invuln > 0.0f) return;

    health = std::max(0.0f, health - amount);
    invuln = 0.6f;
}

sf::Vector2f Tank::aim_direction() const
{
    return to_cartesian({1.0f, sf::degrees(turret_degrees.get_value())});
}

sf::Vector2f Tank::muzzle_position() const
{
    float const kick = 34.0f - recoil * 7.0f;
    return pos + aim_direction() * kick;
}

void Tank::draw(sf::RenderTarget& target) const
{
    // Blink while briefly invulnerable after a hit.
    bool const blink = invuln > 0.0f &&
                       std::fmod(invuln, 0.16f) > 0.08f;

    sf::Color hull_color = COL_HULL;
    if(blink) hull_color = sf::Color(235, 235, 245);

    // Treads, drawn under and slightly wider than the hull.
    sf::RectangleShape treads({RADIUS * 2.2f, RADIUS * 2.0f});
    treads.setOrigin(treads.getSize() / 2.0f);
    treads.setPosition(pos);
    treads.setRotation(hull_angle);
    treads.setFillColor(COL_TREAD);
    target.draw(treads);

    sf::RectangleShape hull({RADIUS * 1.9f, RADIUS * 1.45f});
    hull.setOrigin(hull.getSize() / 2.0f);
    hull.setPosition(pos);
    hull.setRotation(hull_angle);
    hull.setFillColor(hull_color);
    target.draw(hull);

    // Barrel. Rotated by the turret angle, negated because screen y
    // grows downward while our angles treat up as positive.
    float const barrel_len = 34.0f - recoil * 7.0f;
    sf::RectangleShape barrel({barrel_len, 7.0f});
    barrel.setOrigin({2.0f, 3.5f});
    barrel.setPosition(pos);
    barrel.setRotation(sf::degrees(-turret_degrees.get_value()));
    barrel.setFillColor(overheated ? sf::Color(224, 108, 92) : COL_BARREL);
    target.draw(barrel);

    sf::CircleShape turret_cap(11.0f);
    turret_cap.setOrigin({11.0f, 11.0f});
    turret_cap.setPosition(pos);
    turret_cap.setFillColor(blink ? hull_color : COL_BARREL);
    target.draw(turret_cap);
}

// ---------------------------------------------------------------- Bullet

void Bullet::update(sf::Time dt)
{
    pos  += velocity * dt.asSeconds();
    life -= dt.asSeconds();
}

bool Bullet::dead() const
{
    return life <= 0.0f ||
           pos.x < 0.0f || pos.y < 0.0f ||
           pos.x > ARENA_W || pos.y > ARENA_H;
}

// ----------------------------------------------------------------- Enemy

Enemy::Enemy(sf::Vector2f start, Kind k, float difficulty)
    : pos(start), kind(k)
{
    if(kind == Kind::DRONE)
    {
        health = 3.0f;
        radius = 19.0f;
        speed  = 96.0f + difficulty * 12.0f;
    }
    else
    {
        health = 1.0f;
        radius = 13.0f;
        speed  = 168.0f + difficulty * 18.0f;
    }

    // Enemies pop into existence rather than blinking in.
    spawn_scale.set_transition(TransitionFunction::EaseOutBack);
    spawn_scale.set_duration(0.35f);
    spawn_scale.set_instant(0.0f);
    spawn_scale = 1.0f;

    spin = Random::real(0.0f, 360.0f);
}

void Enemy::update(sf::Time dt, sf::Vector2f player_pos)
{
    float const seconds = dt.asSeconds();

    sf::Vector2f const to_player = normalized(player_pos - pos);

    if(kind == Kind::DRONE)
    {
        velocity += to_player * speed * 3.0f * seconds;
    }
    else
    {
        // Runners weave, so they are not a straight line to shoot down.
        sf::Vector2f const side{-to_player.y, to_player.x};
        float const weave = std::sin(GameTime::now() * 3.4f + spin) * 0.7f;
        velocity += (to_player + side * weave) * speed * 3.0f * seconds;
    }

    float const max_speed = speed;
    if(length(velocity) > max_speed) velocity = normalized(velocity) * max_speed;

    pos += velocity * seconds;
    confine(pos, velocity, radius);

    spin      += seconds * (kind == Kind::DRONE ? 70.0f : 190.0f);
    hit_flash  = std::max(0.0f, hit_flash - seconds * 6.0f);
}

sf::Color Enemy::base_color() const
{
    return (kind == Kind::DRONE) ? COL_ENEMY : COL_ENEMY_2;
}

int Enemy::score_value() const
{
    return (kind == Kind::DRONE) ? 100 : 175;
}

void Enemy::draw(sf::RenderTarget& target) const
{
    float const scale = spawn_scale.get_value();
    if(scale <= 0.01f) return;

    sf::Color color = base_color();
    if(hit_flash > 0.0f)
    {
        // Lerp toward white on hit, so shots read as connecting.
        float const t = std::clamp(hit_flash, 0.0f, 1.0f);
        color = sf::Color(
            static_cast<std::uint8_t>(color.r + (255 - color.r) * t),
            static_cast<std::uint8_t>(color.g + (255 - color.g) * t),
            static_cast<std::uint8_t>(color.b + (255 - color.b) * t));
    }

    if(kind == Kind::DRONE)
    {
        sf::CircleShape body(radius * scale, 6);
        body.setOrigin({radius * scale, radius * scale});
        body.setPosition(pos);
        body.setRotation(sf::degrees(spin));
        body.setFillColor(color);
        body.setOutlineThickness(-3.0f);
        body.setOutlineColor(sf::Color(30, 24, 30));
        target.draw(body);
    }
    else
    {
        sf::CircleShape body(radius * scale, 3);
        body.setOrigin({radius * scale, radius * scale});
        body.setPosition(pos);
        body.setRotation(sf::degrees(spin));
        body.setFillColor(color);
        target.draw(body);
    }
}

// ---------------------------------------------------------------- Pickup

void Pickup::update(sf::Time dt)
{
    life -= dt.asSeconds();
    bob  += dt.asSeconds() * 3.0f;
}

bool Pickup::dead() const
{
    return life <= 0.0f;
}

void Pickup::draw(sf::RenderTarget& target) const
{
    float const wobble = std::sin(bob) * 3.0f;

    // Fade out over the last two seconds rather than vanishing.
    float const alpha = std::clamp(life / 2.0f, 0.0f, 1.0f);

    sf::CircleShape body(RADIUS, 4);
    body.setOrigin({RADIUS, RADIUS});
    body.setPosition(pos + sf::Vector2f{0.0f, wobble});
    body.setRotation(sf::degrees(bob * 24.0f));
    body.setFillColor(sf::Color(COL_PICKUP.r, COL_PICKUP.g, COL_PICKUP.b,
                                static_cast<std::uint8_t>(255 * alpha)));
    target.draw(body);
}

// ------------------------------------------------------------ ArenaFloor

ArenaFloor::ArenaFloor()
{
    constexpr float STEP = 80.0f;

    for(float x = 0.0f; x <= ARENA_W; x += STEP)
    {
        m_grid.append(sf::Vertex{{x, 0.0f},    COL_GRID});
        m_grid.append(sf::Vertex{{x, ARENA_H}, COL_GRID});
    }
    for(float y = 0.0f; y <= ARENA_H; y += STEP)
    {
        m_grid.append(sf::Vertex{{0.0f,    y}, COL_GRID});
        m_grid.append(sf::Vertex{{ARENA_W, y}, COL_GRID});
    }

    // Border built from triangles so it has real thickness at any zoom.
    auto add_quad = [this](sf::Vector2f p, sf::Vector2f size, sf::Color c)
    {
        sf::Vector2f const a{p.x, p.y};
        sf::Vector2f const b{p.x + size.x, p.y};
        sf::Vector2f const d{p.x + size.x, p.y + size.y};
        sf::Vector2f const e{p.x, p.y + size.y};

        for(sf::Vector2f v : {a, b, d, a, d, e})
            m_border.append(sf::Vertex{v, c});
    };

    sf::Color const edge(96, 108, 140);
    constexpr float T = 8.0f;

    add_quad({-T, -T},          {ARENA_W + T * 2.0f, T}, edge);
    add_quad({-T, ARENA_H},     {ARENA_W + T * 2.0f, T}, edge);
    add_quad({-T, 0.0f},        {T, ARENA_H},            edge);
    add_quad({ARENA_W, 0.0f},   {T, ARENA_H},            edge);
}

void ArenaFloor::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    target.draw(m_grid, states);
    target.draw(m_border, states);
}

}   // namespace arena
