/**
 * Soldier.hpp  -  Mud Cats
 *
 * One man, on either side. Everything about him is plain data here; what
 * he does is decided in Battle (the player's input, or the AI), and what
 * happens to him when he is hit is decided in Battle's combat code.
 *
 * A soldier is not a bar of hit points. He can be wounded and keep going,
 * lose an arm or a leg, go down and bleed out unless a medic reaches him,
 * burn, or be killed outright. Suppression, how much fire has passed
 * close to him lately, decides as much about what he does as his wounds.
 */

#ifndef MUDCATS_SOLDIER_HPP
#define MUDCATS_SOLDIER_HPP

#include <SFML/Graphics.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace mudcats
{

enum class Team : std::uint8_t { MONTERAN, RECHIVII };
enum class Role : std::uint8_t { RIFLEMAN, OFFICER, MEDIC, FLAMER, GUNNER, RADIOMAN, STORMTROOPER, SNIPER, SAPPER };
inline constexpr int ROLE_COUNT = 9;
enum class Stance : std::uint8_t { STAND, CROUCH, PRONE };
enum class Life : std::uint8_t { OK, DOWNED, DEAD, FLED };
enum class Order : std::uint8_t { FOLLOW, HOLD, ADVANCE, SUPPRESS, CHARGE };
enum class Gun : std::uint8_t { RIFLE, SMG, PISTOL, MG, FLAMER, SNIPER, NONE };

enum Limb : std::uint8_t
{
    ARM_L = 1, ARM_R = 2, LEG_L = 4, LEG_R = 8, HEAD = 16,
};

struct WeaponSpec
{
    const char* name;
    int   magazine;
    int   spare;            // rounds carried besides the magazine
    float interval;         // seconds between shots
    float reload;
    float damage;
    float speed;            // pixels per second
    float range;
    float spread;           // radians, one standard deviation
    float kick;             // added to spread per shot, recovers over time
    bool  automatic;
    bool  flame;
    const char* sound;      // prefix: "rifle" picks rifle_1..3
    const char* far_sound;
    const char* reload_sound;
    int   tracer_every;     // 1 = every round glows
    float suppress;         // how frightening a near miss is
};

[[nodiscard]] const WeaponSpec& weapon_spec(Gun gun);
[[nodiscard]] Gun default_gun(Role role);
[[nodiscard]] const char* role_name(Role role);
[[nodiscard]] const char* gun_name(Gun gun);

struct Soldier
{
    int         id{0};
    Team        team{Team::MONTERAN};
    Role        role{Role::RIFLEMAN};
    Gun         gun{Gun::RIFLE};        // what he carries now; he can pick up another
    std::string name;
    std::string rank;
    int         section{-1};
    bool        leader{false};          // leads his section

    // --- body ------------------------------------------------------------
    sf::Vector2f pos;
    sf::Vector2f vel;
    float  aim{0.0f};               // radians; where the weapon points
    float  body_angle{0.0f};        // for prone and lying figures
    Stance stance{Stance::STAND};
    Life   life{Life::OK};
    float  hp{100.0f};
    float  bleeding{0.0f};          // hp per second
    float  bleed_out{0.0f};         // seconds left once down
    std::uint8_t lost{0};           // Limb bits
    float  burning{0.0f};
    bool   charred{false};
    bool   bandaged{false};
    float  walk_phase{0.0f};
    float  stance_change{0.0f};     // seconds until a stance change finishes

    // --- head --------------------------------------------------------------
    float suppression{0.0f};        // 0..1
    float shock{0.0f};              // a near explosion: stunned, deafened
    float morale{1.0f};

    // --- kit -----------------------------------------------------------------
    int   ammo{0};
    int   spare{0};
    float reloading{0.0f};
    float cooldown{0.0f};
    float kick{0.0f};
    int   grenades{0};
    int   bandages{1};
    int   medkits{0};
    float fuel{0.0f};
    float heat{0.0f};               // machine gun barrel
    float overheated{0.0f};
    bool  deployed{false};
    float deploying{0.0f};
    float deploy_dir{0.0f};
    int   burst_left{0};
    int   shots_fired{0};
    int   smokes{0};
    int   flares{0};
    int   satchels{0};
    int   missions{0};              // fire missions a radioman can still call
    float cutting{0.0f};            // seconds left cutting wire
    sf::Vector2f cut_at;
    int   bags{0};                  // sandbags he can still fill (the hill)
    float digging{0.0f};            // seconds left digging in
    float dig_total{1.0f};
    sf::FloatRect dig_rect;         // where the bags go, or the hole
    bool  dig_hole{false};
    float lunge{0.0f};              // bayonet thrust in progress
    float bayonet_cd{0.0f};
    sf::Vector2f lunge_dir;

    // --- doing -----------------------------------------------------------------
    bool  is_player{false};
    bool  yours{false};             // in the player's section
    bool  firing{false};            // trigger held this frame
    float ready{0.0f};              // 0 weapon lowered, 1 up and aimed
    float flash{0.0f};              // muzzle flash timer
    float revealed{0.0f};           // fired lately: his flash gives him away
    float sway{0.0f};               // AI aim error for the current burst
    int   dragging{-1};             // id of the man being dragged
    int   dragged_by{-1};
    int   treating{-1};             // id of the man being treated
    float treat_time{0.0f};

    // --- AI ----------------------------------------------------------------------
    Order        order{Order::FOLLOW};
    sf::Vector2f goal;
    sf::Vector2f post;              // where a defender holds
    bool         has_post{false};
    sf::Vector2f suppress_at;
    float        suppress_until{0.0f};
    std::vector<sf::Vector2f> path;
    float        repath{0.0f};
    float        think{0.0f};
    int          target{-1};
    float        reaction{0.0f};
    sf::Vector2f threat;
    float        threat_age{99.0f};
    int          cover_index{-1};
    bool         active{true};      // false until the script wakes him
    bool         reserve{false};    // part of the counter-attack
    float        bark_cooldown{0.0f};
    std::string  bark;
    float        bark_time{0.0f};
    float        idle_anim{0.0f};
    bool         sitting{false};
    bool         surrendered{false};
    float        charge_until{0.0f};
    int          want_pickup{-1};   // a dropped weapon he is going for
    float        hold_until{0.0f};  // waits for the men ahead of him before moving off
    bool         alerted{true};     // false: asleep in a dugout, or a sentry who hasn't noticed yet
    sf::Vector2f home;              // where a man who breaks runs to (zero: off to the east)
    int          wave{-1};          // the hill: which wave he came up with
    int          serving{-1};       // crewing a mortar: its index
    bool         support{false};    // the hill: a gun or a sniper hanging back, not in the rush

    // --- record --------------------------------------------------------------------
    int         kills{0};
    std::string fate;

    [[nodiscard]] bool alive() const { return life == Life::OK; }
    [[nodiscard]] bool fighting() const { return life == Life::OK && !surrendered; }
    [[nodiscard]] const WeaponSpec& weapon() const { return weapon_spec(gun); }
    [[nodiscard]] bool has_mg() const { return gun == Gun::MG; }
    [[nodiscard]] bool has_flamer() const { return gun == Gun::FLAMER; }
    [[nodiscard]] bool one_handed() const { return gun == Gun::PISTOL || gun == Gun::SMG; }
    [[nodiscard]] bool cuts_wire() const { return role == Role::SAPPER || role == Role::STORMTROOPER; }
    [[nodiscard]] bool can_shoot() const { return gun != Gun::NONE && (can_use_rifle() || one_handed()); }
    [[nodiscard]] bool can_use_rifle() const { return !(lost & (ARM_L | ARM_R)); }
    [[nodiscard]] bool can_walk() const { return !(lost & (LEG_L | LEG_R)); }
    [[nodiscard]] float radius() const { return stance == Stance::PRONE ? 3.0f : 3.5f; }
};

/*
 * The figure, top-down, in pixels. Living men are drawn every frame; the
 * dead are drawn once into the ground texture and forgotten.
 */
void draw_soldier(sf::VertexArray& va, const Soldier& s, float time);
void draw_shadow(sf::VertexArray& va, const Soldier& s);

/*
 * The same man from the side, for the briefing: standing at ease with his
 * weapon held low, or brought up to the shoulder as `raise` goes to 1.
 * `breath` is time, for the rise and fall of his chest. Feet at (0, 0),
 * one unit a pixel; he is about 30 tall.
 */
void draw_profile(sf::VertexArray& va, Role role, Team team, float breath, float raise);

[[nodiscard]] sf::Color uniform_colour(Team team);

}

#endif
