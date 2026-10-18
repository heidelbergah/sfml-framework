/**
 * Entities.hpp  -  Ironvale Arena
 *
 * Gameplay objects for the demo. Deliberately plain structs with an
 * update() and a draw() -- the framework does not impose an entity
 * model, and for a few hundred objects this is enough.
 *
 * Everything here is in world units per second, matching the particle
 * system's convention.
 */

#ifndef ARENA_ENTITIES_HPP
#define ARENA_ENTITIES_HPP

#include "../../include/Includes.hpp"

namespace arena
{
    inline constexpr float ARENA_W = 2400.0f;
    inline constexpr float ARENA_H = 1600.0f;

    inline const sf::Color COL_HULL   {104, 148, 122};
    inline const sf::Color COL_TREAD  { 58,  74,  68};
    inline const sf::Color COL_BARREL {186, 202, 176};
    inline const sf::Color COL_BULLET {255, 236, 170};
    inline const sf::Color COL_ENEMY  {206,  84,  92};
    inline const sf::Color COL_ENEMY_2{232, 140,  76};
    inline const sf::Color COL_PICKUP {126, 214, 176};
    inline const sf::Color COL_GRID   { 38,  42,  56};

    /*
     * Player tank. The hull turns with A/D and drives with W/S; the
     * turret tracks the mouse independently, which is the whole point
     * of a top-down tank.
     */
    struct Tank
    {
        sf::Vector2f pos{ARENA_W / 2.0f, ARENA_H / 2.0f};
        sf::Vector2f velocity;

        sf::Angle hull_angle;
        Interpolated<float> turret_degrees;

        float health{100.0f};
        float max_health{100.0f};

        float heat{0.0f};          // 0-100, locks the gun at 100
        bool  overheated{false};

        float fire_cooldown{0.0f};
        float recoil{0.0f};        // visual barrel kick, 0-1
        float invuln{0.0f};

        static constexpr float RADIUS       = 26.0f;
        static constexpr float DRIVE_ACCEL  = 620.0f;
        static constexpr float MAX_SPEED    = 300.0f;
        static constexpr float TURN_SPEED   = 150.0f;   // degrees/sec
        static constexpr float FIRE_DELAY   = 0.14f;
        static constexpr float HEAT_PER_SHOT= 9.0f;
        static constexpr float COOL_RATE    = 26.0f;

        Tank();

        void update(sf::Time dt);
        void aim_at(sf::Vector2f world_target);
        [[nodiscard]] bool can_fire() const;
        void on_fired();
        void damage(float amount);

        [[nodiscard]] sf::Vector2f muzzle_position() const;
        [[nodiscard]] sf::Vector2f aim_direction() const;

        void draw(sf::RenderTarget& target) const;
    };

    struct Bullet
    {
        sf::Vector2f pos;
        sf::Vector2f velocity;
        float        life{1.6f};

        static constexpr float SPEED  = 1050.0f;
        static constexpr float RADIUS = 4.0f;

        void update(sf::Time dt);
        [[nodiscard]] bool dead() const;
    };

    /*
     * Two flavours: DRONE chases directly, RUNNER is faster but weaker
     * and drifts. Enough variety to make the arena read as designed
     * rather than random.
     */
    struct Enemy
    {
        enum class Kind { DRONE, RUNNER };

        sf::Vector2f pos;
        sf::Vector2f velocity;
        Kind         kind{Kind::DRONE};

        float health{2.0f};
        float radius{18.0f};
        float speed{110.0f};
        float spin{0.0f};
        float hit_flash{0.0f};

        Interpolated<float> spawn_scale;

        Enemy(sf::Vector2f start, Kind k, float difficulty);

        void update(sf::Time dt, sf::Vector2f player_pos);
        void draw(sf::RenderTarget& target) const;

        [[nodiscard]] sf::Color base_color() const;
        [[nodiscard]] int       score_value() const;
    };

    struct Pickup
    {
        sf::Vector2f pos;
        float        life{14.0f};
        float        bob{0.0f};

        static constexpr float RADIUS = 13.0f;

        void update(sf::Time dt);
        void draw(sf::RenderTarget& target) const;
        [[nodiscard]] bool dead() const;
    };

    /*
     * Static backdrop: a grid plus a border, built once into a vertex
     * array rather than drawn line by line every frame.
     */
    class ArenaFloor : public sf::Drawable
    {
    private:
        sf::VertexArray m_grid{sf::PrimitiveType::Lines};
        sf::VertexArray m_border{sf::PrimitiveType::Triangles};

        void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

    public:
        ArenaFloor();
    };

    [[nodiscard]] bool circles_overlap(sf::Vector2f a, float ra,
                                       sf::Vector2f b, float rb);

    /*
     * Keeps a position inside the arena and zeroes the velocity
     * component that pushed it out.
     */
    void confine(sf::Vector2f& pos, sf::Vector2f& velocity, float radius);
}

#endif
