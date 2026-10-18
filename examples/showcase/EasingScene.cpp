/**
 * EasingScene.cpp  -  Showcase
 */

#include "EasingScene.hpp"

#include <cstdio>

namespace showcase
{

namespace
{
    // The track is shorter than the screen on purpose: out elastic
    // overshoots by 37% of the distance travelled, in back and in out
    // elastic by 10-12% the other way. Overshoot zones either side keep
    // every curve on screen and clear of the labels.
    constexpr float ZONE_LEFT   = 204.0f;
    constexpr float ZONE_RIGHT  = 940.0f;
    constexpr float TRACK_LEFT  = 362.0f;
    constexpr float TRACK_RIGHT = 782.0f;
    constexpr float LANE_TOP    = 86.0f;
    constexpr float LANE_STEP   = 32.0f;
    constexpr float PUCK        = 10.0f;
    constexpr std::size_t TRAIL = 12;

    constexpr float MIN_DURATION = 0.2f;
    constexpr float MAX_DURATION = 3.0f;
}

EasingScene::EasingScene()
    : DemoScene("Easing playground", "click to retarget   space  auto")
{
    struct Curve { TransitionFunction curve; const char* name; sf::Color color; };
    Curve const curves[] = {
        {TransitionFunction::Linear,               "linear",         TEXT},
        {TransitionFunction::EaseInQuad,           "in quad",        SKY},
        {TransitionFunction::EaseOutQuad,          "out quad",       SKY},
        {TransitionFunction::EaseInOutQuad,        "in out quad",    SKY},
        {TransitionFunction::EaseInOutExponential, "in out expo",    ROSE},
        {TransitionFunction::EaseInBack,           "in back",        AMBER},
        {TransitionFunction::EaseOutBack,          "out back",       AMBER},
        {TransitionFunction::EaseInOutBack,        "in out back",    AMBER},
        {TransitionFunction::EaseOutElastic,       "out elastic",    CORAL},
        {TransitionFunction::EaseInOutElastic,     "in out elastic", CORAL},
        {TransitionFunction::EaseOutBounce,        "out bounce",     SAGE},
    };

    for(const Curve& c : curves)
    {
        Lane lane{c.curve, c.color, Interpolated<float>{TRACK_LEFT}, {}};
        lane.x.set_transition(c.curve);
        m_lanes.push_back(lane);

        auto label = make_label({170.0f, LANE_STEP}, c.name, TEXT_DIM, Alignment::RIGHT);
        label->set_position({24.0f, lane_y(m_lanes.size() - 1) - LANE_STEP / 2.0f}, true);
        m_widgets.add_widget(std::string("lane_") + c.name, label);
    }

    float const controls_y = lane_y(m_lanes.size() - 1) + 34.0f;

    auto duration_title = make_label({80.0f, 24.0f}, "duration", TEXT_DIM, Alignment::RIGHT);
    duration_title->set_position({114.0f, controls_y}, true);
    m_widgets.add_widget("duration_title", duration_title);

    m_duration = std::make_shared<Slider>(sf::Vector2f{220.0f, 14.0f}, AMBER);
    m_duration->set_body_color(LINE);
    m_duration->set_transition(TransitionFunction::None);
    m_duration->set_position({ZONE_LEFT + 10.0f, controls_y + 5.0f}, true);
    m_duration->set_percentage((1.2f - MIN_DURATION) / (MAX_DURATION - MIN_DURATION));
    m_widgets.add_widget("duration", m_duration);

    m_duration_label = make_label({60.0f, 24.0f}, "", TEXT);
    m_duration_label->set_position({ZONE_LEFT + 242.0f, controls_y}, true);
    m_widgets.add_widget("duration_label", m_duration_label);

    m_auto_button = make_button({110.0f, 24.0f}, "auto  on");
    m_auto_button->set_position({ZONE_RIGHT - 110.0f, controls_y}, true);
    m_auto_button->set_callback([this]() { set_auto(!m_auto); });
    m_widgets.add_widget("auto", m_auto_button);

    apply_duration();
    rebuild_tracks();
}

float EasingScene::lane_y(std::size_t index) const
{
    return LANE_TOP + LANE_STEP * static_cast<float>(index);
}

bool EasingScene::in_lane_area(sf::Vector2f point) const
{
    float const top    = LANE_TOP - LANE_STEP / 2.0f;
    float const bottom = lane_y(m_lanes.size() - 1) + LANE_STEP / 2.0f;
    return point.y >= top && point.y <= bottom &&
           point.x >= ZONE_LEFT && point.x <= ZONE_RIGHT;
}

void EasingScene::on_enter()
{
    DemoScene::on_enter();

    for(Lane& lane : m_lanes)
    {
        lane.x.set_instant(TRACK_LEFT);
        lane.trail.clear();
    }

    m_at_right = false;
    m_hold     = 0.3f;
    set_auto(true);
}

void EasingScene::retarget(float x)
{
    float const clamped = std::clamp(x, TRACK_LEFT, TRACK_RIGHT);

    // Assigning is all it takes: each lane starts a fresh tween from its
    // current, possibly overshooting, position.
    for(Lane& lane : m_lanes) lane.x = clamped;
}

void EasingScene::set_auto(bool on)
{
    m_auto = on;
    m_auto_button->add_text(m_auto ? "auto  on" : "auto  off", *font(),
                            m_auto ? TEXT : TEXT_DIM);
}

void EasingScene::apply_duration()
{
    float const seconds = m_duration->get_value(MIN_DURATION, MAX_DURATION);
    for(Lane& lane : m_lanes) lane.x.set_duration(seconds);

    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%.1fs", static_cast<double>(seconds));
    m_duration_label->set_string(buffer);
}

void EasingScene::rebuild_tracks()
{
    m_tracks.clear();

    for(std::size_t i = 0; i < m_lanes.size(); ++i)
    {
        float const y = lane_y(i);
        push_rect(m_tracks, {{ZONE_LEFT, y}, {ZONE_RIGHT - ZONE_LEFT, 1.0f}}, PANEL);
        push_rect(m_tracks, {{TRACK_LEFT, y}, {TRACK_RIGHT - TRACK_LEFT, 1.0f}}, PANEL_HI);
        push_rect(m_tracks, {{TRACK_LEFT, y - 3.0f},  {1.0f, 7.0f}}, LINE);
        push_rect(m_tracks, {{TRACK_RIGHT, y - 3.0f}, {1.0f, 7.0f}}, LINE);
    }
}

void EasingScene::handle_event(const sf::Event& event, sf::Vector2f mouse_pos)
{
    DemoScene::handle_event(event, mouse_pos);

    if(const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
    {
        // Click, not drag: retargeting every mouse move would restart
        // the back curves' wind-up from an already wound-up position,
        // and they would creep further backwards with each event.
        if(pressed->button == sf::Mouse::Button::Left && in_lane_area(mouse_pos))
        {
            set_auto(false);
            retarget(mouse_pos.x);
        }
    }
    else if(const auto* key = event.getIf<sf::Event::KeyPressed>())
    {
        if(key->code == sf::Keyboard::Key::Space) set_auto(!m_auto);
    }
}

void EasingScene::update(sf::Time delta_time)
{
    DemoScene::update(delta_time);

    if(m_duration->value_changed()) apply_duration();

    // Auto mode: once every lane has landed, pause a beat and send them
    // all to the other end.
    if(m_auto)
    {
        bool const all_landed = std::all_of(m_lanes.begin(), m_lanes.end(),
            [](const Lane& lane) { return lane.x.is_transition_over(); });

        if(all_landed)
        {
            m_hold -= delta_time.asSeconds();
            if(m_hold <= 0.0f)
            {
                m_at_right = !m_at_right;
                retarget(m_at_right ? TRACK_RIGHT : TRACK_LEFT);
                m_hold = 0.6f;
            }
        }
    }

    // Draw each puck with a short trail behind it. The trail is what
    // makes the shape of each curve readable: bunched where the curve
    // is slow, stretched where it is fast, past the end where it
    // overshoots.
    m_pucks.clear();
    for(std::size_t i = 0; i < m_lanes.size(); ++i)
    {
        Lane& lane    = m_lanes[i];
        float const y = lane_y(i);
        float const x = lane.x;

        lane.trail.push_front(x);
        if(lane.trail.size() > TRAIL) lane.trail.pop_back();

        for(std::size_t t = lane.trail.size(); t-- > 1;)
        {
            float const age  = static_cast<float>(t) / static_cast<float>(TRAIL);
            float const size = std::floor(PUCK * (1.0f - age * 0.6f));
            sf::Vector2f const at = floor_vector({lane.trail[t] - size / 2.0f,
                                                  y - size / 2.0f + 0.5f});
            push_rect(m_pucks, {at, {size, size}}, fade(lane.color, 0.35f * (1.0f - age)));
        }

        // Where this lane is heading.
        float const target = lane.x.get_target();
        push_rect(m_pucks, {floor_vector({target, y - 6.0f}), {1.0f, 13.0f}},
                  fade(lane.color, 0.45f));

        sf::Vector2f const at = floor_vector({x - PUCK / 2.0f, y - PUCK / 2.0f + 0.5f});
        push_rect(m_pucks, {at, {PUCK, PUCK}}, lane.color);
    }
}

void EasingScene::render(sf::RenderTarget& target)
{
    target.setView(target.getDefaultView());
    target.draw(m_tracks);
    target.draw(m_pucks);
    target.draw(m_widgets);
}

}
