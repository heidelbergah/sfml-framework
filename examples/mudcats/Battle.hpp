/**
 * Battle.hpp  -  Mud Cats
 *
 * A platoon's day: twenty-five Monterans, a lieutenant and three sections,
 * against about forty Rechivii. Or a raid at night, a squad of eleven, or
 * a hill to hold for as long as anyone is left. Four places to have it:
 *
 *   The Ochre line   in the rain. Behind the lines first, the range, the walk
 *                    up to the front; then the whistle, no man's land under a
 *                    machine gun, their trench, a bunker, a farm to hold.
 *   Halvek fields    at dawn, in the mist. The platoon marches up a sunken lane
 *                    and runs into them: shots from a wood, hedgerow to
 *                    hedgerow, a stream, a mill at the bridge to take and keep.
 *   The Dravo raid   before dawn, in the dark. Eleven stormtroopers creep up
 *                    to the wire, get through it, and clear a trench, a
 *                    hamlet and a command dugout at close range, then get
 *                    back before it is light.
 *   Hill 312         from the afternoon into the night. Sixteen of us dug in
 *                    on a hilltop; they come up it in waves, more each time,
 *                    from more sides, with guns, mortars and flamethrowers,
 *                    until there is nobody left to hold it.
 *
 * You play one man. If he dies you take over the nearest of the others,
 * until there are none left.
 *
 * The class is big, so it is split by subject across five files:
 *
 *   Battle.cpp    setup, input, the platoon and its sections, the levels
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
inline constexpr std::size_t  MAX_SOLDIERS = 128;

/*
 * What the briefing chose, and what the debrief reads back.
 */
struct Campaign
{
    Role    chosen{Role::RIFLEMAN};
    LevelId level{LevelId::TRENCHES};
    int     difficulty{1};              // 0 easy, 1 normal, 2 hard
    bool    won{false};
    float   time{0.0f};
    int     enemy_dead{0};
    int     enemy_fled{0};
    int     prisoners{0};
    int     prisoners_shot{0};
    int     waves{0};                   // the hill: waves beaten off
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
        // The Ochre line
        REAR, RANGE, FRONT, SCOUT, BARRAGE, ASSAULT, TRENCH, BUNKER, HOLD,
        // Halvek fields
        MARCH, CONTACT, WOOD, STREAM, MILL,
        // The Dravo raid
        RAID_CREEP, RAID_TRENCH, RAID_HAMLET, RAID_DUGOUT, RAID_WITHDRAW,
        // Hill 312
        HILL_LULL, HILL_WAVE,
        // Both
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
        int          belts_cut{0};      // the raid: how many belts of wire its lane is through
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

    // Someone not here yet: the next wave, coming on in its own time.
    struct Pending
    {
        float at{0.0f};
        Role  role{Role::RIFLEMAN};
        sf::Vector2f pos;
        sf::Vector2f home;
        int   section{-1};
        bool  leader{false};
        int   kind{0};                  // 0 the rush, 1 a gun giving cover, 2 a sniper, 3 mortar gunner, 4 mortar loader, 5 one of ours
        sf::Vector2f post;
        int   mortar{-1};
        std::string name;
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

    // --- the world ----------------------------------------------------------------
    Map               m_map;
    LevelId           m_level{LevelId::TRENCHES};
    sf::RenderTexture m_ground;         // the field, and everything that happened on it
    sf::RenderTexture m_frame;          // 480x270, then graded and scaled up
    sf::RenderTexture m_shade;          // what the player can't see
    sf::Shader*       m_grade{nullptr};
    sf::VertexArray   m_stamps{sf::PrimitiveType::Triangles};
    bool              m_ready{false};
    LevelId           m_built{LevelId::TRENCHES};
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
    std::vector<Pending> m_pending;

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
    Stage m_stage{Stage::REAR};
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
    float m_mist{1.0f};                 // 1 thick, 0 lifted
    std::string m_objective;
    std::string m_hint;
    float m_hint_time{0.0f};
    int   m_bottles_left{3};
    bool  m_officer_dead_noted{false};
    float m_rout_check{0.0f};
    float m_night{0.0f};                // 1 full dark
    bool  m_alarm{true};                // the raid: false until they know we're there
    float m_illum_timer{0.0f};
    float m_alarm_at{0.0f};
    int   m_creep{0};                   // the raid, before the alarm: 0 forming up, 1 cutting, 2 through, 3 at the lip
    float m_keep_up{0.0f};              // the corporal waiting for you
    int   m_bound_wait{-1};             // a section about to bound, once the covering fire is going
    float m_cover_until{0.0f};
    bool  m_assault{false};             // the waiting bound is at a holdout, not the next waypoint
    sf::Vector2f m_assault_at;
    // The hill.
    int   m_wave{0};                    // the wave coming, or on us
    int   m_waves_held{0};
    std::vector<float> m_wave_dirs;     // where the current wave comes from, as angles from the crest
    float m_last_threat{0.0f};          // the main direction of the last one
    int   m_hill_dead{0};               // theirs, killed, whose places have been taken by new men
    int   m_hill_fled{0};
    int   m_new_men{0};                 // replacements so far
    float m_night_target{0.0f};
    bool  m_wave_broke{false};
    float m_wave_go{0.0f};              // when the rush goes in, once everyone is at the foot of the hill
    static constexpr float FIRST_LULL = 45.0f;    // to dig in before the first wave
    static constexpr float LULL = 10.0f;          // between waves

    // --- setup and flow (Battle.cpp) ---------------------------------------------
    void build_world();
    void spawn_trenches();
    void spawn_fields();
    void spawn_raid();
    void fill_roles(int section);
    void pick_player(int section);
    void kit_out(Soldier& s);
    Soldier& add(Team team, Role role, std::string rank, std::string name, sf::Vector2f pos, int section);
    int  add_section(Team team, std::string name, float band);
    void set_stage(Stage stage);
    void update_script(float dt);
    void script_trenches(float dt);
    void script_fields(float dt);
    void script_raid(float dt);
    void spawn_hill();
    void script_hill(float dt);
    void plan_wave();
    void spawn_pending();
    void hill_positions(float facing, float second);
    Soldier* spawn_soldier(Team team, Role role, std::string rank, std::string name, sf::Vector2f pos, int section);
    void place_mortar(sf::Vector2f at, float facing, int gunner, int loader);
    void update_mortars(float dt);
    void start_dig(Soldier& s, bool hole, sf::Vector2f toward);
    void finish_dig(Soldier& s);
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

    // --- helpers -----------------------------------------------------------------------------
    [[nodiscard]] Soldier* player();
    [[nodiscard]] const Soldier& me() const;
    [[nodiscard]] Soldier* officer(Team team);
    [[nodiscard]] Soldier* section_leader(int section);
    [[nodiscard]] int count(Team team) const;
    [[nodiscard]] bool player_leads() const;
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
