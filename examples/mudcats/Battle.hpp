/**
 * Battle.hpp  -  Mud Cats
 *
 * A platoon's fight, played from either side of it. Three places to have it:
 *
 *   The Sava meadow  a grey morning. A hundred and forty Rechivii with bayonets
 *                    fixed come across an open field at thirty Monterans in a
 *                    shallow trench.
 *   Kalvo            at dusk. A market town, a main street of terraced houses
 *                    up to a square and a church with a gun in its tower.
 *                    House to house, block by block.
 *   The Lesna road   midday, in the forest. A Monteran supply column on a
 *                    track through the trees, and a Rechivii ambush waiting
 *                    for it.
 *
 * You play one man. If he dies you take over the nearest of the others,
 * until there are none left.
 *
 * The class is big, so it is split by subject across these files:
 *
 *   Battle.cpp    setup, input, the platoon and its sections, the stages
 *   Charge.cpp    the Sava meadow
 *   Town.cpp      Kalvo
 *   Forest.cpp    the Lesna road
 *   Mortar.cpp    their mortars
 *   Combat.cpp    weapons, bullets, wounds, gore, grenades, fire, shells
 *   Effects.cpp   everything drawn with the framework's particle systems
 *   AI.cpp        how everyone who isn't you decides what to do
 *   Render.cpp    the field, the people, the effects and the HUD
 */

#ifndef MUDCATS_BATTLE_HPP
#define MUDCATS_BATTLE_HPP

#include "../../include/scenes/Scene.hpp"
#include "../../include/scenes/SceneManager.hpp"

#include "../../include/framework/Camera.hpp"
#include "../../include/framework/ParticleSystemManager.hpp"

#include "Map.hpp"
#include "Soldier.hpp"

#include <bitset>
#include <deque>
#include <string>
#include <vector>

namespace mudcats
{

inline constexpr sf::Vector2u FRAME{480u, 270u};
inline constexpr sf::Vector2f FRAME_F{480.0f, 270.0f};
inline constexpr std::size_t  MAX_SOLDIERS = 256;

/*
 * What the briefing chose, and what the debrief reads back.
 */
struct Campaign
{
    Role    chosen{Role::RIFLEMAN};
    LevelId level{LevelId::CHARGE};
    int     difficulty{1};              // 0 easy, 1 normal, 2 hard
    bool    won{false};
    float   time{0.0f};
    int     enemy_dead{0};
    int     enemy_fled{0};
    int     prisoners{0};
    int     prisoners_shot{0};
    Team    side{Team::MONTERAN};       // which side the briefing has picked: the charge can be played from either
    Team    playing{Team::MONTERAN};    // the side this battle is played from
    std::vector<Soldier>     platoon;   // copies, at the end
    std::vector<std::string> log;       // what happened, in order

    static Campaign& current()
    {
        static Campaign c;
        return c;
    }
};

[[nodiscard]] const char* difficulty_name(int d);
[[nodiscard]] const char* level_name(LevelId level);

class Battle : public Scene
{
public:
    enum class Stage
    {
        // The Sava meadow
        CHARGE_FORM, CHARGE_WALK, CHARGE_RUN,
        // Kalvo
        TOWN_ENTER, TOWN_STREET, TOWN_CROSS, TOWN_SQUARE,
        // The Lesna road
        FOREST_MARCH, FOREST_AMBUSH, FOREST_PUSH,
        // All of them
        COUNTER, WON, LOST,
    };

private:
    // --- sections ------------------------------------------------------------
    struct Section
    {
        int          id{0};
        Team         team{Team::MONTERAN};
        std::string  name;
        Order        order{Order::HOLD};
        sf::Vector2f point;             // where it was sent
        float        band{0.0f};        // its share of the line, across the advance
        float        since{0.0f};       // seconds under the current order
        sf::Vector2f suppress_at;
        float        suppress_until{0.0f};
        int          at{0};             // the waypoint it holds
        int          heading{0};        // the waypoint it is bounding to
        bool         bounding{false};
        float        bound_start{0.0f};
    };

    // --- things in the world ---------------------------------------------------
    struct Bullet
    {
        sf::Vector2f pos, vel, origin;
        sf::Vector2f prev;              // where it was last frame, for its streak
        float range{0.0f};
        float damage{0.0f};
        float suppress{0.0f};
        int   owner{-1};
        Team  team{Team::MONTERAN};
        bool  tracer{false};
        bool  heavy{false};
        std::bitset<MAX_SOLDIERS> passed;   // soldiers it has already frightened
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
        bool  smoke{false};
        bool  satchel{false};           // a sapper's charge: heavy, short fuse, goes through walls
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

    struct Smoke
    {
        sf::Vector2f pos;
        float radius{4.0f};
        float life{24.0f};
        float spawn{0.0f};
    };

    struct Flare
    {
        sf::Vector2f pos, vel, target;
        float height{3.0f}, vz{90.0f};
        float life{0.0f};
        bool  fired{false};
        int   owner{-1};
        bool  illum{false};             // a white star to see by, not a signal for the guns
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

    struct Dropped                      // a weapon on the ground
    {
        sf::Vector2f pos;
        Gun   gun{Gun::RIFLE};
        int   ammo{0};
        int   spare{0};
        float fuel{0.0f};
        float angle{0.0f};
        bool  taken{false};
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
        int   kind{0};                  // 0 theirs, 1 our barrage on their line, 2 called by flare, 3 called by radio, 4 their mortar
        bool  smoke{false};
    };

    // A fire mission called on the radio: a ranging round first, then,
    // once the radioman has seen where it fell, the full salvo.
    struct Mission
    {
        int          caller{-1};
        Team         team{Team::MONTERAN};
        sf::Vector2f target;
        int          step{0};           // 0 calling, 1 ranging round in the air, 2 waiting for the correction, 3 fire for effect
        float        timer{0.0f};
        bool         smoke{false};
    };

    // A mortar: a tube on a baseplate and the two men who serve it. It
    // drops its bombs on whatever the riflemen in front can see, walking
    // them on round by round, and every round's flash gives it away.
    struct Mortar
    {
        sf::Vector2f pos;
        float facing{3.14159f};
        int   gunner{-1}, loader{-1};
        int   rounds{30};
        float cooldown{8.0f};
        sf::Vector2f aim_at;
        float error{80.0f};
        float seen{0.0f};               // seconds left that its flash marks it
        float kick{0.0f};               // the tube jumping, for drawing
        bool  silenced{false};
    };

    // A wagon in the forest column: a load of shells and rations, two horses.
    struct Wagon
    {
        float        along{0.0f};       // how far along the road
        sf::Vector2f pos;
        sf::Vector2f dir{1.0f, 0.0f};
        float        hp{100.0f};
        float        burning{0.0f};     // seconds it has been on fire
        bool         destroyed{false};
        bool         halted{false};
        float        horses_down{-1.0f};    // when the horses fall, after it's sprung
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
        float pitch{1.0f};              // the man's voice, for a cry
    };

    // --- the world ----------------------------------------------------------------
    Map               m_map;
    LevelId           m_level{LevelId::CHARGE};
    sf::RenderTexture m_ground;         // the field, and everything that happened on it
    sf::RenderTexture m_frame;          // 480x270, then graded and scaled up
    sf::RenderTexture m_shade;          // what the player can't see
    sf::Shader*       m_grade{nullptr};
    sf::VertexArray   m_stamps{sf::PrimitiveType::Triangles};
    bool              m_ready{false};
    LevelId           m_built{LevelId::CHARGE};
    bool              m_built_once{false};

    Camera m_camera;
    const sf::Font* m_font{nullptr};

    std::vector<Soldier> m_soldiers;
    std::vector<Section> m_sections;
    std::vector<Bullet>  m_bullets;
    std::vector<Grenade> m_grenades;
    std::vector<Flame>   m_flames;
    std::vector<Fire>    m_fires;
    std::vector<Smoke>   m_smokes;
    std::vector<Flare>   m_flares;
    std::vector<Gib>     m_gibs;
    std::vector<Dropped> m_dropped;
    std::vector<Light>   m_lights;
    std::vector<Shell>   m_shells;
    std::deque<Line>     m_lines;
    std::vector<Later>   m_later;
    std::vector<Mortar>  m_mortars;
    std::vector<Wagon>   m_wagons;

    // --- particles (Effects.cpp) ---------------------------------------------------
    ParticleSystemManager           m_fx;
    std::shared_ptr<ParticleSystem> m_smoke_fx;     // soft grey squares that grow and fade
    std::shared_ptr<ParticleSystem> m_fire_fx;      // additive, white to red
    std::shared_ptr<ParticleSystem> m_spark_fx;     // additive streaks
    std::shared_ptr<ParticleSystem> m_blood_fx;     // thrown up, stains where it lands
    std::shared_ptr<ParticleSystem> m_dirt_fx;      // the same, for earth
    std::shared_ptr<ParticleSystem> m_trail_fx;     // tracer glow and bullet wakes
    std::shared_ptr<ParticleSystem> m_rain_fx;
    std::shared_ptr<ParticleSystem> m_screen_fx;    // smoke screens: big, slow, opaque
    std::shared_ptr<ParticleSystem> m_round_fx;     // rounds in flight: hot additive streaks
    std::shared_ptr<ParticleSystem> m_glow_fx;      // soft additive halos: tracers, flares, muzzle blasts
    sf::RenderTexture m_light;                      // at night: how much of the frame each light reaches

    // --- the player ----------------------------------------------------------------
    int  m_player{-1};
    sf::Vector2f m_mouse_screen;        // base-resolution pixels
    sf::Vector2f m_mouse_world;
    sf::Vector2f m_look;                // where the camera leans, eased
    bool m_up{false}, m_down{false}, m_left{false}, m_right{false};
    bool m_sprint{false};
    bool m_trigger{false};
    bool m_trigger_pressed{false};      // this frame, for bolt actions
    bool m_aiming{false};
    bool m_paused{false};
    int  m_sound_row{0};                // the pause screen: which level the arrow keys change
    int  m_sound_drag{-1};              // the level being dragged with the mouse, if any
    // Where the pause screen draws each sound level's bar, in base-resolution pixels.
    [[nodiscard]] static sf::FloatRect sound_bar(int row) { return {{150.0f, 196.0f + 34.0f * static_cast<float>(row)}, {170.0f, 8.0f}}; }
    void set_sound_level(int row, float value);
    bool m_autoplay{false};             // MUDCATS_AUTOPLAY: your man fights on his own
    int  m_selected{-1};                // the section your orders go to; -1 is all of them
    float m_hurt{0.0f};                 // red at the edges after a hit
    float m_flash{0.0f};                // white after a close explosion
    float m_takeover{0.0f};
    std::string m_takeover_text;
    float m_bandage_time{0.0f};
    float m_dead_wait{0.0f};
    float m_notice{0.0f};
    std::string m_notice_text;
    sf::Vector2f m_kick;                // the camera nudged by a kill
    bool  m_smoke_mission{false};       // the radioman's next call: smoke or HE
    std::vector<Mission> m_missions;

    // --- the script -------------------------------------------------------------------
    Stage m_stage{Stage::CHARGE_FORM};
    float m_stage_time{0.0f};
    float m_time{0.0f};
    float m_real{0.0f};
    int   m_waypoint{0};
    int   m_bounding{0};                // which section moves next
    float m_waypoint_time{0.0f};
    float m_barrage_timer{0.0f};
    float m_far_guns{3.0f};
    float m_end_timer{0.0f};
    float m_combat_heat{0.0f};          // recent shooting, for the music
    std::string m_objective;
    std::string m_hint;
    float m_hint_time{0.0f};
    bool  m_officer_dead_noted{false};
    bool  m_section_hint_given{false};  // told the player his section is his to lead
    float m_rout_check{0.0f};
    float m_night{0.0f};                // 1 full dark
    bool  m_alarm{true};                // the forest: false until the ambush is sprung
    float m_alarm_at{0.0f};
    float m_illum_timer{0.0f};
    float m_keep_up{0.0f};              // the corporal waiting for you
    int   m_bound_wait{-1};             // a section about to bound, once the covering fire is going
    float m_cover_until{0.0f};
    bool  m_assault{false};             // the waiting bound is at a holdout, not the next waypoint
    sf::Vector2f m_assault_at;
    // The charge.
    int   m_charge_men{0};              // how many stepped off
    float m_second_line{0.0f};          // when the second line steps off
    bool  m_second_off{false};
    bool  m_colours_down{false};
    bool  m_roared{false};              // the second roar, close in
    int   m_colours_taken{0};
    // The town.
    int   m_town_sections[5]{};          // theirs: the first houses, the second block, the square, the church, the reserve
    int   m_town_start{0};              // how many of us went in
    float m_town_check{0.0f};
    bool  m_town_broken{false};         // the attack has given up
    float m_town_break_time{0.0f};
    float m_bell_timer{0.0f};
    int   m_bells{0};
    // The forest.
    float m_column{0.0f};               // how far along the road the lead wagon is
    int   m_forest_rch{0};              // how many of them lay in wait
    int   m_forest_mon{0};              // how many of us were with the column
    float m_forest_check{0.0f};
    int   m_forest_outcome{0};          // 0 still going, 1 the column through, 2 the column lost
    float m_forest_outcome_at{0.0f};

    // --- setup and flow (Battle.cpp) ---------------------------------------------
    void build_world();
    void fill_roles(int section);
    void pick_player(int section);
    void kit_out(Soldier& s);
    Soldier& add(Team team, Role role, std::string rank, std::string name, sf::Vector2f pos, int section);
    int  add_section(Team team, std::string name, float band);
    void set_stage(Stage stage);
    void update_script(float dt);
    void spawn_charge();
    void script_charge(float dt);
    void charge_think(Soldier& s);
    void spawn_town();
    void script_town(float dt);
    void town_fall_back(int from, int to);
    void town_reinforce(int men);
    void spawn_forest();
    void script_forest(float dt);
    void forest_shortcut(int stage);
    void halt_column();
    void update_wagons(float dt);
    void damage_wagons(sf::Vector2f at, float power, bool fire);
    void ignite_wagon(Wagon& w);
    void draw_wagons(sf::VertexArray& va) const;
    bool raider_think(Soldier& s);
    [[nodiscard]] int wagons_lost() const;
    [[nodiscard]] Team ours() const;            // the side you're on
    void place_mortar(sf::Vector2f at, float facing, int gunner, int loader);
    void update_mortars(float dt);
    void raise_alarm(const std::string& why);
    [[nodiscard]] bool you_there(const sf::FloatRect& area, float reach) const;
    void leapfrog(Soldier& lt, int limit);
    void platoon_orders(float dt);
    void order_section(int section, Order order, sf::Vector2f point, sf::Vector2f suppress_at = {});
    void order_from_player(Order order);
    void promote(int section);
    void say(const Soldier& who, const std::string& text, float time = 3.5f);
    void say(const std::string& who, const std::string& text, Team team = Team::MONTERAN, float time = 3.5f);
    void hint(const std::string& text, float time = 6.0f);
    void notice(const std::string& text, float time = 3.0f);
    void bark(Soldier& s, const std::string& text, bool voice = true);
    void log(const std::string& line);
    void player_input(float dt);
    void interact(Soldier& p);
    void take_over();
    void finish();

    // --- combat (Combat.cpp) -------------------------------------------------------------
    void fire(Soldier& s, float dt);
    void fire_bullet(Soldier& s, float angle);
    void reload(Soldier& s);
    void throw_grenade(Soldier& s, sf::Vector2f at, bool smoke = false);
    void fire_flare(Soldier& s, sf::Vector2f at, bool illum = false);
    void call_fire_mission(Soldier& s, sf::Vector2f at);
    void update_missions(float dt);
    void place_satchel(Soldier& s, sf::Vector2f at);
    void cut_wire_at(Soldier& s, sf::Vector2f at);
    void bayonet(Soldier& s);
    void update_bayonet(Soldier& s, float dt);
    void drop_weapon(Soldier& s);
    bool pick_up(Soldier& s, Dropped& d);
    void update_soldier_body(Soldier& s, float dt);
    void update_world(float dt);
    void update_bullets(float dt);
    void hit(Soldier& s, const Bullet& b, sf::Vector2f at);
    void wound(Soldier& s, float damage, sf::Vector2f from_dir, int attacker, int cause, bool heavy);
    void sever(Soldier& s, Limb limb, sf::Vector2f push);
    void gib_body(Soldier& s, sf::Vector2f push);
    void go_down(Soldier& s, int attacker);
    void kill(Soldier& s, int attacker, const std::string& fate);
    void surrender(Soldier& s);
    void explode(sf::Vector2f at, float power, int owner, Team team, bool fire, const std::string& cause, bool spare_monterans = false);
    void ignite(Soldier& s, int attacker);
    void stamp_corpse(Soldier& s);
    void hear_shot(const Soldier& shooter);

    // --- effects (Effects.cpp) ---------------------------------------------------------
    void setup_effects();
    void spray_blood(sf::Vector2f at, sf::Vector2f dir, int count, float speed);
    void dust(sf::Vector2f at, int count, sf::Color colour);
    void muzzle(const Soldier& s);
    void puff(sf::Vector2f at, sf::Vector2f vel, float size, float grow, float life, sf::Color colour);
    void flame_lick(sf::Vector2f at, sf::Vector2f vel, float size, float life);
    void sparks(sf::Vector2f at, int count, float speed, sf::Color colour, float life = 0.25f);
    void debris(sf::Vector2f at, int count, float power);
    void update_effects(float dt);

    // --- AI (AI.cpp) ---------------------------------------------------------------------
    void think(Soldier& s, float dt);
    void steer(Soldier& s, float dt);
    void act(Soldier& s, float dt);
    int  find_target(const Soldier& s) const;
    int  pick_cover(const Soldier& s, sf::Vector2f around, float radius, sf::Vector2f threat) const;
    void set_goal(Soldier& s, sf::Vector2f goal, float radius = 60.0f);
    void section_goal(Soldier& s);
    void medic_logic(Soldier& s);
    bool scavenge(Soldier& s);

    // --- drawing (Render.cpp) --------------------------------------------------------------
    void draw_world(sf::RenderTarget& target, const sf::View& view);
    void draw_shade(const sf::View& view);
    void draw_hud(sf::RenderTarget& target);
    void draw_crosshair(sf::RenderTarget& frame);
    void draw_markers(sf::RenderTarget& target);
    void draw_lights(const sf::View& view);
    [[nodiscard]] std::string interact_prompt() const;
    void flush_stamps();
    void compute_visibility();
    std::vector<std::uint8_t> m_visible;    // per soldier, this frame
    // Men you can hear and not see: a shot, a shout, boots on boards. Where
    // the sound came from, roughly, and until when it's marked.
    struct Heard { int id; sf::Vector2f pos; float since; float until; };
    std::vector<Heard> m_heard;
    void listen(const Soldier& s, float d);

    // --- helpers -----------------------------------------------------------------------------
    [[nodiscard]] Soldier* player();
    [[nodiscard]] const Soldier& me() const;
    [[nodiscard]] Soldier* officer(Team team);
    [[nodiscard]] Soldier* section_leader(int section);
    [[nodiscard]] int count(Team team) const;
    [[nodiscard]] bool player_leads() const;            // you are the lieutenant: the platoon is yours
    [[nodiscard]] bool player_commands_section() const; // you lead your section, and only it
    Soldier* acting_commander();                        // the senior corporal, with the lieutenant gone
    [[nodiscard]] bool visible_to_player(const Soldier& s) const;
    [[nodiscard]] bool sight(sf::Vector2f a, sf::Vector2f b) const;
    [[nodiscard]] bool through_smoke(sf::Vector2f a, sf::Vector2f b) const;
    [[nodiscard]] bool can_spot(const Soldier& viewer, const Soldier& target, float distance) const;
    [[nodiscard]] float vision_range() const;
    [[nodiscard]] float spread_of(const Soldier& s) const;
    [[nodiscard]] float enemy_accuracy() const;
    [[nodiscard]] bool fighting_started() const;
    [[nodiscard]] float lit(sf::Vector2f p) const;
    [[nodiscard]] int player_section() const;
    [[nodiscard]] sf::Vector2f objective_point() const;
    [[nodiscard]] sf::Vector2f forest_objective() const;
    [[nodiscard]] int forest_limit() const;
    [[nodiscard]] sf::Vector2f forest_forward() const;
    [[nodiscard]] sf::Vector2f edge_marker(sf::Vector2f world) const;   // screen point for an off-screen marker

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
