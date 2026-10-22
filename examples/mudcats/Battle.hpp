/**
 * Battle.hpp  -  Mud Cats
 *
 * The demo level: one attack, eight Monterans against sixteen Rechivii.
 *
 * It starts quietly, behind the lines, so you can learn to walk and aim
 * before anything shoots back: a reserve trench, a firing range with
 * bottles on crates, a walk up the communication trench to the front.
 * Then a scout, a barrage, the whistle, and the assault: across no man's
 * land under a machine gun, into their trench, past a bunker, and into a
 * farmhouse you have to hold against their counter-attack.
 *
 * You play one man. If he dies you take over the nearest of the others,
 * until there are none left.
 *
 * The class is big, so it is split by subject across four files:
 *
 *   Battle.cpp    setup, input, the level script, the squad's orders
 *   Combat.cpp    weapons, bullets, wounds, gore, grenades, fire, shells
 *   AI.cpp        how everyone who is not you decides what to do
 *   Render.cpp    the field, the people, the effects and the HUD
 */

#ifndef MUDCATS_BATTLE_HPP
#define MUDCATS_BATTLE_HPP

#include "../../include/scenes/Scene.hpp"
#include "../../include/scenes/SceneManager.hpp"

#include "../../include/framework/Camera.hpp"

#include "Map.hpp"
#include "Soldier.hpp"

#include <deque>
#include <string>
#include <vector>

namespace mudcats
{

inline constexpr sf::Vector2u FRAME{480u, 270u};
inline constexpr sf::Vector2f FRAME_F{480.0f, 270.0f};

/*
 * What the briefing chose, and what the debrief reads back.
 */
struct Campaign
{
    Role chosen{Role::RIFLEMAN};
    bool won{false};
    float time{0.0f};
    int  enemy_dead{0};
    int  enemy_fled{0};
    std::vector<Soldier> squad;     // copies, at the end

    static Campaign& current()
    {
        static Campaign c;
        return c;
    }
};

class Battle : public Scene
{
public:
    enum class Stage
    {
        REAR,           // behind the lines
        RANGE,          // on the firing range
        FRONT,          // in the front trench, waiting
        SCOUT,          // a Rechivii scout at the wire
        BARRAGE,        // our guns on their line, then the whistle
        ASSAULT,        // across no man's land
        TRENCH,         // their trench is ours; on to the bunker
        BUNKER,         // the bunker is clear; on to the farm
        HOLD,           // in the farmhouse, waiting
        COUNTER,        // they are coming back
        WON,
        LOST,
    };

private:
    // --- effects ------------------------------------------------------------
    struct Bullet
    {
        sf::Vector2f pos, vel, origin;
        float range{0.0f};
        float damage{0.0f};
        float suppress{0.0f};
        int   owner{-1};
        Team  team{Team::MONTERAN};
        bool  tracer{false};
        bool  heavy{false};
        std::uint64_t passed{0};        // soldiers it has already frightened
        bool  dead{false};
    };

    struct Grenade
    {
        sf::Vector2f pos, vel;
        float height{4.0f}, vz{0.0f};
        float fuse{3.0f};
        int   owner{-1};
        Team  team{Team::MONTERAN};
        float spin{0.0f};
    };

    struct Flame
    {
        sf::Vector2f pos, vel;
        float life{0.0f}, max_life{0.5f};
        int   owner{-1};
        Team  team{Team::MONTERAN};
    };

    struct Fire
    {
        sf::Vector2f pos;
        float radius{6.0f};
        float life{6.0f};
        float spawn{0.0f};
    };

    struct Gib
    {
        sf::Vector2f pos, vel;
        float height{3.0f}, vz{0.0f};
        float angle{0.0f}, spin{0.0f};
        int   kind{0};                  // 0 chunk, 1 arm, 2 leg, 3 head, 4 helmet
        Team  team{Team::MONTERAN};
        bool  charred{false};
        float trail{0.0f};
    };

    struct Drop
    {
        sf::Vector2f pos, vel;
        float height{3.0f}, vz{0.0f};
        float size{1.0f};
        bool  dirt{false};
    };

    struct Puff
    {
        sf::Vector2f pos, vel;
        float life{1.0f}, max_life{1.0f};
        float size{3.0f}, grow{4.0f};
        sf::Color colour;
    };

    struct Spark
    {
        sf::Vector2f pos, vel;
        float life{0.3f};
        sf::Color colour;
    };

    struct Light
    {
        sf::Vector2f pos;
        float radius{20.0f};
        float life{0.1f}, max_life{0.1f};
        sf::Color colour;
    };

    struct Shell
    {
        sf::Vector2f target;
        float time{2.0f};
        bool  whistled{false};
        bool  ours{false};              // our guns: harmless show on their line
    };

    struct Line
    {
        std::string who;
        std::string text;
        float time{4.0f};
        Team  team{Team::MONTERAN};
    };

    struct Later                        // a sound a moment from now
    {
        float t{0.0f};
        std::string name;
        sf::Vector2f pos;
        float volume{1.0f};
    };

    struct Popup
    {
        sf::Vector2f pos;
        std::string  text;
        float life{1.5f};
        sf::Color colour;
    };

    // --- the world ----------------------------------------------------------------
    Map               m_map;
    sf::RenderTexture m_ground;         // the field, and everything that happened on it
    sf::RenderTexture m_frame;          // 480x270, then graded and scaled up
    sf::RenderTexture m_shade;          // what the player cannot see
    sf::Shader*       m_grade{nullptr};
    sf::VertexArray   m_stamps{sf::PrimitiveType::Triangles};
    bool              m_ready{false};

    Camera m_camera;
    const sf::Font* m_font{nullptr};

    std::vector<Soldier> m_soldiers;
    std::vector<Bullet>  m_bullets;
    std::vector<Grenade> m_grenades;
    std::vector<Flame>   m_flames;
    std::vector<Fire>    m_fires;
    std::vector<Gib>     m_gibs;
    std::vector<Drop>    m_drops;
    std::vector<Puff>    m_puffs;
    std::vector<Spark>   m_sparks;
    std::vector<Light>   m_lights;
    std::vector<Shell>   m_shells;
    std::deque<Line>     m_lines;
    std::vector<Popup>   m_popups;
    std::vector<Later>   m_later;

    // --- the player ----------------------------------------------------------------
    int  m_player{-1};
    sf::Vector2f m_mouse_screen;        // base-resolution pixels
    sf::Vector2f m_mouse_world;
    bool m_up{false}, m_down{false}, m_left{false}, m_right{false};
    bool m_sprint{false};
    bool m_trigger{false};
    bool m_trigger_pressed{false};      // this frame, for bolt actions
    bool m_aiming{false};
    bool m_paused{false};
    bool m_autoplay{false};             // MUDCATS_AUTOPLAY: your man fights on his own
    float m_hurt{0.0f};                 // red at the edges after a hit
    float m_flash{0.0f};                // white after a close explosion
    float m_takeover{0.0f};
    std::string m_takeover_text;
    float m_bandage_time{0.0f};
    float m_dead_wait{0.0f};

    // --- the script -------------------------------------------------------------------
    Stage m_stage{Stage::REAR};
    float m_stage_time{0.0f};
    float m_time{0.0f};
    float m_real{0.0f};
    int   m_waypoint{0};
    float m_waypoint_time{0.0f};
    float m_barrage_timer{0.0f};
    float m_far_guns{3.0f};
    float m_end_timer{0.0f};
    float m_intensity{0.0f};
    float m_combat_heat{0.0f};          // recent shooting, for the music
    std::string m_objective;
    std::string m_hint;
    float m_hint_time{0.0f};
    int   m_bottles_left{3};
    bool  m_officer_dead_noted{false};
    float m_rout_check{0.0f};

    // --- setup and flow (Battle.cpp) ---------------------------------------------
    void build_world();
    void spawn_squads();
    Soldier& add(Team team, Role role, std::string rank, std::string name, sf::Vector2f pos);
    void set_stage(Stage stage);
    void update_script(float dt);
    void squad_orders(float dt);
    void say(const Soldier& who, const std::string& text, float time = 3.5f);
    void say(const std::string& who, const std::string& text, Team team = Team::MONTERAN, float time = 3.5f);
    void hint(const std::string& text, float time = 6.0f);
    void bark(Soldier& s, const std::string& text, bool voice = true);
    void player_input(float dt);
    void take_over();
    void finish();

    // --- combat (Combat.cpp) -------------------------------------------------------------
    void fire(Soldier& s, float dt);
    void fire_bullet(Soldier& s, float angle);
    void reload(Soldier& s);
    void throw_grenade(Soldier& s, sf::Vector2f at);
    void update_soldier_body(Soldier& s, float dt);
    void update_effects(float dt);
    void update_bullets(float dt);
    void hit(Soldier& s, const Bullet& b, sf::Vector2f at);
    void wound(Soldier& s, float damage, sf::Vector2f from_dir, int attacker, int cause, bool heavy);
    void sever(Soldier& s, Limb limb, sf::Vector2f push);
    void gib_body(Soldier& s, sf::Vector2f push);
    void go_down(Soldier& s, int attacker);
    void kill(Soldier& s, int attacker, const std::string& fate);
    void explode(sf::Vector2f at, float power, int owner, Team team, bool fire, const std::string& cause);
    void ignite(Soldier& s, int attacker);
    void spray_blood(sf::Vector2f at, sf::Vector2f dir, int count, float speed);
    void dust(sf::Vector2f at, int count, sf::Color colour);
    void stamp_corpse(Soldier& s);
    void suppress_near(const Bullet& b, sf::Vector2f a, sf::Vector2f c);
    void hear_shot(const Soldier& shooter);

    // --- AI (AI.cpp) ---------------------------------------------------------------------
    void think(Soldier& s, float dt);
    void steer(Soldier& s, float dt);
    void act(Soldier& s, float dt);
    int  find_target(const Soldier& s) const;
    int  pick_cover(const Soldier& s, sf::Vector2f around, float radius, sf::Vector2f threat) const;
    void set_goal(Soldier& s, sf::Vector2f goal);
    bool can_see(const Soldier& a, const Soldier& b) const;
    void medic_logic(Soldier& s);

    // --- drawing (Render.cpp) --------------------------------------------------------------
    void draw_world(sf::RenderTarget& target, const sf::View& view);
    void draw_shade(const sf::View& view);
    void draw_hud(sf::RenderTarget& target);
    void draw_crosshair(sf::RenderTarget& frame);
    void flush_stamps();

    // --- helpers -----------------------------------------------------------------------------
    [[nodiscard]] Soldier* player();
    [[nodiscard]] Soldier* officer(Team team);
    [[nodiscard]] int count(Team team, bool alive_only = true) const;
    [[nodiscard]] bool visible_to_player(const Soldier& s) const;

public:
    Battle();

    void on_enter() override;
    void on_exit() override;
    void handle_event(const sf::Event& event, sf::Vector2f mouse_pos) override;
    void update(sf::Time delta_time) override;
    void update_realtime(sf::Time delta_time) override;
    void render(sf::RenderTarget& target) override;
    [[nodiscard]] sf::Color get_clear_color() const override;
};

}

#endif
