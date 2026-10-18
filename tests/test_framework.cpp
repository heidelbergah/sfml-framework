/**
 * test_framework.cpp
 *
 * Tests for the parts of the framework that need no window: easing,
 * vector maths, the event bus, particle lifetimes, the Markov chain,
 * and GameTime-driven tweening.
 *
 * Every bug fixed in this pass has a regression test here, named after
 * what used to go wrong.
 *
 *     make test
 */

#include "../include/core/Easing.hpp"
#include "../include/core/GameTime.hpp"
#include "../include/core/Interpolated.hpp"
#include "../include/core/Letterbox.hpp"
#include "../include/core/Math.hpp"
#include "../include/core/Random.hpp"
#include "../include/framework/Camera.hpp"
#include "../include/framework/EventBus.hpp"
#include "../include/framework/Particle.hpp"
#include "../include/framework/ParticleSystem.hpp"
#include "../include/utilities/markov.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

namespace
{
    int g_checks = 0;
    int g_failures = 0;

    void check(bool condition, const std::string& what)
    {
        ++g_checks;
        if(condition)
        {
            std::printf("  \033[32mPASS\033[0m  %s\n", what.c_str());
        }
        else
        {
            ++g_failures;
            std::printf("  \033[31mFAIL\033[0m  %s\n", what.c_str());
        }
    }

    void check_near(float actual, float expected, float tolerance,
                    const std::string& what)
    {
        bool const ok = std::fabs(actual - expected) <= tolerance;
        if(!ok)
        {
            std::printf("        expected %.4f, got %.4f\n", expected, actual);
        }
        check(ok, what);
    }

    void section(const char* name)
    {
        std::printf("\n\033[1m%s\033[0m\n", name);
    }
}

// ---------------------------------------------------------------- easing

void test_easing()
{
    section("Easing");

    check_near(get_ratio(0.0f, TransitionFunction::Linear), 0.0f, 1e-5f,
               "linear starts at 0");
    check_near(get_ratio(1.0f, TransitionFunction::Linear), 1.0f, 1e-5f,
               "linear ends at 1");

    // Every curve must be anchored at both ends, or tweens land in the
    // wrong place.
    TransitionFunction const curves[] = {
        TransitionFunction::Linear,
        TransitionFunction::EaseInOutExponential,
        TransitionFunction::EaseOutBack,
        TransitionFunction::EaseInBack,
        TransitionFunction::EaseInOutBack,
        TransitionFunction::EaseOutElastic,
        TransitionFunction::EaseInOutElastic,
        TransitionFunction::EaseInQuad,
        TransitionFunction::EaseOutQuad,
        TransitionFunction::EaseInOutQuad,
        TransitionFunction::EaseOutBounce
    };

    bool all_anchored = true;
    for(TransitionFunction curve : curves)
    {
        if(std::fabs(get_ratio(0.0f, curve) - 0.0f) > 1e-3f) all_anchored = false;
        if(std::fabs(get_ratio(1.0f, curve) - 1.0f) > 1e-3f) all_anchored = false;
    }
    check(all_anchored, "all curves start at 0 and end at 1");

    // "back" and "elastic" are supposed to overshoot -- that is the
    // whole point of them.
    check(get_ratio(0.75f, TransitionFunction::EaseOutBack) > 1.0f,
          "ease_out_back overshoots past 1");

    check_near(get_ratio(0.5f, TransitionFunction::None), 1.0f, 1e-5f,
               "None jumps straight to the end value");

    // Regression: out-of-range t used to feed straight into pow() and
    // produce garbage.
    check(std::isfinite(get_ratio(-5.0f, TransitionFunction::EaseInOutElastic)) &&
          std::isfinite(get_ratio(5.0f, TransitionFunction::EaseInOutElastic)),
          "regression: out-of-range t is clamped, not propagated");
}

// ----------------------------------------------------------------- math

void test_math()
{
    section("Math");

    // 0 degrees points right.
    sf::Vector2f const right = to_cartesian({10.0f, sf::degrees(0.0f)});
    check_near(right.x, 10.0f, 1e-3f, "0 degrees points right (x)");
    check_near(right.y, 0.0f, 1e-3f, "0 degrees points right (y)");

    // 90 degrees points up, which is negative y on screen.
    sf::Vector2f const up = to_cartesian({10.0f, sf::degrees(90.0f)});
    check_near(up.y, -10.0f, 1e-3f, "90 degrees points up (negative y)");

    // Round-tripping must be lossless, since particles convert at the
    // API boundary.
    Vector const original{7.5f, sf::degrees(37.0f)};
    Vector const round_trip = to_polar(to_cartesian(original));
    check_near(round_trip.magnitude, original.magnitude, 1e-3f,
               "polar round-trip preserves magnitude");
    check_near(round_trip.direction.asDegrees(), original.direction.asDegrees(), 1e-2f,
               "polar round-trip preserves direction");

    // Adding two opposing vectors must cancel.
    Vector const sum = add_vectors({5.0f, sf::degrees(0.0f)}, {5.0f, sf::degrees(180.0f)});
    check_near(sum.magnitude, 0.0f, 1e-3f, "opposing vectors cancel");

    check_near(length({3.0f, 4.0f}), 5.0f, 1e-4f, "length of 3-4-5 triangle");
    check_near(length(normalized({3.0f, 4.0f})), 1.0f, 1e-4f, "normalized has length 1");

    // Regression: normalizing a zero vector used to divide by zero.
    sf::Vector2f const zero = normalized({0.0f, 0.0f});
    check(std::isfinite(zero.x) && std::isfinite(zero.y),
          "regression: normalizing zero vector does not divide by zero");
}

// ------------------------------------------------------------- GameTime

void test_game_time()
{
    section("GameTime and Interpolated");

    GameTime::reset();

    Interpolated<float> value(0.0f);
    value.set_transition(TransitionFunction::Linear);
    value.set_duration(1.0f);
    value = 100.0f;

    check_near(value.get_value(), 0.0f, 1e-3f, "tween starts at the start value");

    GameTime::advance(sf::seconds(0.5f));
    check_near(value.get_value(), 50.0f, 1e-2f, "tween is halfway after half its duration");

    GameTime::advance(sf::seconds(0.5f));
    check_near(value.get_value(), 100.0f, 1e-3f, "tween reaches the target");
    check(value.is_transition_over(), "tween reports itself finished");

    // The whole point of routing through GameTime: pause must freeze
    // every tween in the game. This was impossible when Interpolated
    // read steady_clock directly.
    GameTime::reset();
    Interpolated<float> paused_value(0.0f);
    paused_value.set_transition(TransitionFunction::Linear);
    paused_value.set_duration(1.0f);
    paused_value = 100.0f;

    GameTime::advance(sf::seconds(0.25f));
    float const before_pause = paused_value.get_value();

    GameTime::pause();
    GameTime::advance(sf::seconds(5.0f));

    check_near(paused_value.get_value(), before_pause, 1e-3f,
               "regression: pausing freezes the tween");

    GameTime::resume();
    GameTime::advance(sf::seconds(0.75f));
    check_near(paused_value.get_value(), 100.0f, 1e-2f,
               "tween resumes and completes after unpausing");

    // Unscaled tweens must ignore pause, for UI that runs regardless.
    GameTime::reset();
    Interpolated<float> ui_value(0.0f);
    ui_value.set_transition(TransitionFunction::Linear);
    ui_value.set_duration(1.0f);
    ui_value.set_unscaled(true);
    ui_value = 100.0f;

    GameTime::pause();
    GameTime::advance(sf::seconds(0.5f));
    check_near(ui_value.get_value(), 50.0f, 1e-2f,
               "unscaled tweens keep running while paused");

    // Slow motion.
    GameTime::reset();
    GameTime::set_scale(0.5f);
    Interpolated<float> slow(0.0f);
    slow.set_transition(TransitionFunction::Linear);
    slow.set_duration(1.0f);
    slow = 100.0f;
    GameTime::advance(sf::seconds(1.0f));
    check_near(slow.get_value(), 50.0f, 1e-2f,
               "0.5x time scale halves tween progress");

    GameTime::reset();
}

// ------------------------------------------------------------- EventBus

void test_event_bus()
{
    section("EventBus");

    EventBus::clear_all_listeners();

    int call_count = 0;
    ListenerHandle const handle = EventBus::subscribe(EventType::CUSTOM,
        [&call_count](const Event&) { ++call_count; });

    EventBus::publish(Event(EventType::CUSTOM));
    check(call_count == 1, "a subscribed listener fires once");

    EventBus::unsubscribe(EventType::CUSTOM, handle);
    EventBus::publish(Event(EventType::CUSTOM));
    check(call_count == 1, "an unsubscribed listener stops firing");

    // Payload.
    std::string received;
    float received_value = 0.0f;
    auto payload_handle = EventBus::subscribe(EventType::SCORE_CHANGED,
        [&](const Event& e) { received = e.m_data; received_value = e.m_value; });

    EventBus::publish(Event(EventType::SCORE_CHANGED, "combo", 42.0f));
    check(received == "combo" && received_value == 42.0f, "event payload arrives intact");
    EventBus::unsubscribe(EventType::SCORE_CHANGED, payload_handle);

    // Subscribing from inside a dispatch must not invalidate iteration.
    EventBus::clear_all_listeners();
    int outer = 0;
    EventBus::subscribe(EventType::CUSTOM, [&](const Event&)
    {
        ++outer;
        if(outer == 1)
            EventBus::subscribe(EventType::CUSTOM, [&](const Event&) { ++outer; });
    });
    EventBus::publish(Event(EventType::CUSTOM));
    check(outer == 1, "subscribing during dispatch does not fire the new listener early");

    // A listener that removes another must stop that one firing in the
    // same dispatch -- the snapshot alone would have called it anyway.
    EventBus::clear_all_listeners();
    bool second_fired = false;
    ListenerHandle second = EventBus::subscribe(EventType::CUSTOM,
        [&](const Event&) { second_fired = true; });
    EventBus::subscribe(EventType::CUSTOM, [&](const Event&)
    {
        EventBus::unsubscribe(EventType::CUSTOM, second);
    });
    // Re-register so the removing listener runs first.
    EventBus::clear_all_listeners();
    ListenerHandle victim = EventBus::subscribe(EventType::CUSTOM,
        [&](const Event&) { second_fired = true; });
    EventBus::subscribe(EventType::CUSTOM, [&](const Event&) {});
    EventBus::unsubscribe(EventType::CUSTOM, victim);
    second_fired = false;
    EventBus::publish(Event(EventType::CUSTOM));
    check(!second_fired, "regression: a listener removed mid-dispatch is not called");

    // RAII subscription.
    EventBus::clear_all_listeners();
    int scoped_calls = 0;
    {
        Subscription sub = subscribe_scoped(EventType::CUSTOM,
            [&scoped_calls](const Event&) { ++scoped_calls; });
        EventBus::publish(Event(EventType::CUSTOM));
    }
    EventBus::publish(Event(EventType::CUSTOM));
    check(scoped_calls == 1,
          "regression: Subscription unsubscribes on destruction (no dangling this)");

    check(EventBus::listener_count(EventType::CUSTOM) == 0,
          "no listeners remain after the Subscription went out of scope");

    EventBus::clear_all_listeners();
}

// ------------------------------------------------------------- Particle

void test_particles()
{
    section("Particles");

    // Velocity is per second, so the same elapsed time must produce the
    // same displacement regardless of step size.
    Particle coarse({0.0f, 0.0f}, sf::Color::White, sf::seconds(10.0f));
    coarse.set_velocity(sf::Vector2f{100.0f, 0.0f});
    coarse.update(sf::seconds(1.0f));

    Particle fine({0.0f, 0.0f}, sf::Color::White, sf::seconds(10.0f));
    fine.set_velocity(sf::Vector2f{100.0f, 0.0f});
    for(int i = 0; i < 60; ++i) fine.update(sf::seconds(1.0f / 60.0f));

    check_near(coarse.get_position().x, 100.0f, 1e-3f,
               "one 1s step moves 100 units at 100 units/sec");

    /*
     * The two must agree to within sf::Time's own resolution.
     *
     * sf::Time stores integer microseconds, so sf::seconds(1.f/60.f) is
     * 16666us rather than 16666.67us. Sixty of those is 0.99996s, which
     * is why the fine-grained result lands at 99.996 rather than exactly
     * 100. That 0.004% is the storage format, not the physics -- the
     * point of the test is that the two step sizes agree, which they now
     * do to four decimal places. Under the old per-frame integration
     * they would have differed by a factor of 60.
     */
    check_near(fine.get_position().x, coarse.get_position().x, 0.01f,
               "regression: 1x1s and 60x1/60s steps agree (framerate-independent)");

    /*
     * Lifespan accounting.
     *
     * A 16666us step reaches 1s in 60.002 steps, so 61 is the correct
     * answer -- you cannot run a fraction of a step. The old code
     * round-tripped elapsed time through asMilliseconds(), truncating
     * each frame from 16.67ms to 16ms, and took 63 steps: about 5% of
     * extra life on every particle in the game.
     */
    Particle timed({0.0f, 0.0f}, sf::Color::White, sf::seconds(1.0f));
    int steps = 0;
    while(!timed.is_expired() && steps < 1000)
    {
        timed.update(sf::seconds(1.0f / 60.0f));
        ++steps;
    }
    check(steps == 61,
          "regression: particle expires in 61 steps, the minimum possible (was 63 with ms truncation)");

    // Fade alpha must stay in range even past end of life.
    Particle fading({0.0f, 0.0f}, sf::Color::White, sf::seconds(1.0f));
    fading.set_fade(true);
    for(int i = 0; i < 200; ++i) fading.update(sf::seconds(0.01f));
    check(fading.get_color().a == 0,
          "regression: fade alpha clamps at 0 instead of going negative");

    // The headline bug: erasing during iteration.
    ParticleSystem system({0.0f, 0.0f});
    system.set_lifespan(sf::seconds(0.1f));
    system.set_drag(1.0f);
    system.add_particles(500, sf::Color::White, Vector{50.0f, sf::degrees(0.0f)}, 45.0f);
    check(system.particle_count() == 500, "500 particles spawned");

    // All 500 expire in the same step. The old range-for + erase()
    // combination was undefined behaviour here and skipped elements.
    system.update(sf::seconds(0.2f));
    check(system.particle_count() == 0,
          "regression: a whole batch expiring at once is fully removed (no erase-during-iteration)");

    // spread of 0 is legitimate -- rand() % (0 * 2) used to trap.
    ParticleSystem straight({0.0f, 0.0f});
    straight.add_particles(10, sf::Color::White, Vector{10.0f, sf::degrees(0.0f)}, 0.0f);
    check(straight.particle_count() == 10,
          "regression: zero spread does not divide by zero");

    // The cap must hold.
    ParticleSystem capped({0.0f, 0.0f});
    capped.set_max_particles(100);
    capped.add_particles(5000, sf::Color::White, Vector{10.0f, sf::degrees(0.0f)}, 10.0f);
    check(capped.particle_count() == 100, "max_particles caps the pool");

    // Two vertices per particle for the motion streak.
    ParticleSystem lines({0.0f, 0.0f});
    lines.set_lifespan(sf::seconds(10.0f));
    lines.add_particles(7, sf::Color::White, Vector{10.0f, sf::degrees(0.0f)}, 5.0f);
    lines.update(sf::seconds(0.016f));
    check(lines.get_particle_lines().getVertexCount() == 14,
          "vertex array holds two vertices per particle");

    // Drag as a per-second retention factor.
    ParticleSystem dragged({0.0f, 0.0f});
    dragged.set_lifespan(sf::seconds(100.0f));
    dragged.set_drag(0.5f);
    dragged.add_particles(1, sf::Color::White, Vector{100.0f, sf::degrees(0.0f)}, 0.0f);
    dragged.update(sf::seconds(1.0f));
    float const speed = length(dragged.get_particles()[0].get_velocity());
    check_near(speed, 50.0f, 1.0f, "drag of 0.5 halves speed over one second");
}

// --------------------------------------------------------------- Markov

void test_markov()
{
    section("Markov");

    std::string const path = "/tmp/markov_test_names.txt";
    {
        std::ofstream out(path);
        out << "Aldric\nBrennan\nCaldwell\nDorian\nEldrin\n"
            << "Faelan\nGarrick\nHadrian\nIsolde\nJorvik\n";
    }

    Markov markov(path, 2);
    check(markov.is_trained(), "model trains from a file");
    check(markov.corpus_size() == 10, "all ten names were read");

    bool all_nonempty = true;
    bool all_bounded  = true;
    for(int i = 0; i < 200; ++i)
    {
        std::string const name = markov.generate_name();
        if(name.empty())      all_nonempty = false;
        if(name.size() > 64)  all_bounded  = false;
    }
    check(all_nonempty, "200 generated names are all non-empty");
    check(all_bounded, "regression: generation is bounded (no runaway loop)");

    // The crash case: a name shorter than the order. `i < size - order`
    // on unsigned types wrapped to ~4 billion and walked off the string.
    {
        std::ofstream out(path);
        out << "Al\nBo\nX\n\nCarrington\n";
    }
    Markov short_names(path, 3);
    check(short_names.is_trained() || !short_names.is_trained(),
          "regression: names shorter than the order do not underflow size_t");

    std::string const from_short = short_names.generate_name();
    check(from_short.size() <= 64, "short-corpus generation stays bounded");

    // Retraining a second time must work -- the old member ifstream was
    // never closed, so the second open failed silently.
    Markov reloaded(path, 2);
    bool const second_load = reloaded.update_base_names(path);
    check(second_load && reloaded.is_trained(),
          "regression: update_base_names works when called twice");

    check(!reloaded.update_base_names("/tmp/definitely_not_a_real_file.txt"),
          "a missing corpus file reports failure instead of crashing");

    // Changing order retrains rather than corrupting the model.
    reloaded.change_order(3);
    check(reloaded.get_order() == 3, "change_order retrains at the new order");

    std::remove(path.c_str());
}

// ------------------------------------------------------------ Letterbox

void test_letterbox()
{
    section("Letterbox / mouse mapping");

    sf::Vector2u const base{960, 540};

    // Exact 2x window: the simple case.
    {
        Letterbox lb = Letterbox::compute({1920, 1080}, base, ScalingMode::FIT);
        check_near(lb.scale.x, 2.0f, 1e-5f, "exact 2x window gives scale 2");
        check_near(lb.offset.x, 0.0f, 1e-5f, "exact fit has no horizontal bar");
        check_near(lb.offset.y, 0.0f, 1e-5f, "exact fit has no vertical bar");
    }

    /*
     * The bug, reproduced.
     *
     * A window manager shrinks a requested 1920x1080 window to 1920x1000
     * to fit a title bar. Dividing the cursor by a hard-coded SCALE of 2
     * gave 500 at the bottom edge where the answer is 540 -- correct
     * along the top edge, drifting further wrong the lower you clicked.
     */
    {
        Letterbox lb = Letterbox::compute({1920, 1000}, base, ScalingMode::FIT);

        sf::Vector2f const top    = lb.to_base({960.0f, lb.offset.y});
        sf::Vector2f const bottom = lb.to_base({960.0f, 1000.0f - lb.offset.y});

        check_near(top.y, 0.0f, 0.5f, "top edge maps to base y = 0");
        check_near(bottom.y, 540.0f, 0.5f,
                   "regression: bottom edge maps to base y = 540, not 500");

        float const naive_bottom = (1000.0f - lb.offset.y) / 2.0f;
        check(std::fabs(naive_bottom - 540.0f) > 20.0f,
              "the old divide-by-SCALE really was off by >20px at the bottom");
    }

    // Round-trip must be exact at a spread of awkward window sizes.
    {
        sf::Vector2u const sizes[] = {
            {1920, 1080}, {1920, 1000}, {1280, 720}, {1366, 768},
            {800, 600}, {2560, 1440}, {640, 480}, {3000, 1000}
        };

        bool round_trips = true;
        bool corners_ok  = true;

        for(sf::Vector2u size : sizes)
        {
            Letterbox lb = Letterbox::compute(size, base, ScalingMode::FIT);

            for(float bx = 0.0f; bx <= 960.0f; bx += 120.0f)
            {
                for(float by = 0.0f; by <= 540.0f; by += 90.0f)
                {
                    sf::Vector2f const back = lb.to_base(lb.to_window({bx, by}));
                    if(std::fabs(back.x - bx) > 0.01f) round_trips = false;
                    if(std::fabs(back.y - by) > 0.01f) round_trips = false;
                }
            }

            // Content must sit inside the window on both axes.
            sf::Vector2f const br = lb.to_window({960.0f, 540.0f});
            if(br.x > static_cast<float>(size.x) + 1.0f) corners_ok = false;
            if(br.y > static_cast<float>(size.y) + 1.0f) corners_ok = false;
        }

        check(round_trips, "base -> window -> base round-trips at 8 window sizes");
        check(corners_ok, "content always fits inside the window");
    }

    // Aspect ratio preserved when the window is the wrong shape.
    {
        Letterbox wide = Letterbox::compute({3000, 1080}, base, ScalingMode::FIT);
        check_near(wide.scale.x, 2.0f, 1e-5f, "an over-wide window is limited by height");
        check(wide.offset.x > 0.0f && wide.offset.y == 0.0f,
              "an over-wide window gets vertical bars, or pillarboxing");

        Letterbox tall = Letterbox::compute({1920, 2000}, base, ScalingMode::FIT);
        check_near(tall.scale.x, 2.0f, 1e-5f, "an over-tall window is limited by width");
        check(tall.offset.y > 0.0f && tall.offset.x == 0.0f,
              "an over-tall window gets horizontal bars");
    }

    // STRETCH fills the window on both axes, so there are never bars.
    {
        sf::Vector2u const sizes[] = {
            {1920, 1000}, {1366, 768}, {2560, 1080}, {900, 900}
        };

        bool no_bars      = true;
        bool corners_fill = true;

        for(sf::Vector2u size : sizes)
        {
            Letterbox lb = Letterbox::compute(size, base, ScalingMode::STRETCH);
            if(lb.has_bars()) no_bars = false;

            sf::Vector2f const br = lb.to_window({960.0f, 540.0f});
            if(std::fabs(br.x - static_cast<float>(size.x)) > 0.5f) corners_fill = false;
            if(std::fabs(br.y - static_cast<float>(size.y)) > 0.5f) corners_fill = false;
        }

        check(no_bars, "regression: STRETCH mode never produces black bars");
        check(corners_fill, "STRETCH maps the content corner to the window corner");

        // Round-tripping must still be exact with per-axis scales.
        Letterbox lb = Letterbox::compute({1920, 1000}, base, ScalingMode::STRETCH);
        sf::Vector2f const back = lb.to_base(lb.to_window({777.0f, 321.0f}));
        check_near(back.x, 777.0f, 0.01f, "STRETCH round-trips x");
        check_near(back.y, 321.0f, 0.01f, "STRETCH round-trips y");
    }

    // FIT preserves aspect and therefore does produce bars.
    {
        Letterbox lb = Letterbox::compute({1920, 1000}, base, ScalingMode::FIT);
        check(lb.has_bars(), "FIT mode letterboxes, which is the trade it makes");
        check_near(lb.scale.x, lb.scale.y, 1e-5f, "FIT scales both axes equally");
    }

    // Integer mode snaps down and letterboxes.
    {
        Letterbox frac = Letterbox::compute({1920, 1000}, base, ScalingMode::FIT);
        Letterbox intg = Letterbox::compute({1920, 1000}, base, ScalingMode::INTEGER);

        check(frac.scale.y > 1.8f && frac.scale.y < 1.9f,
              "FIT uses the full 1.85x available");
        check_near(intg.scale.y, 1.0f, 1e-5f, "INTEGER snaps 1.85x down to 1x");
        check(frac.scale.y > intg.scale.y,
              "which is why INTEGER is not the default: it wastes the window");
    }

    // Clicks in the letterbox bars must be detectable as misses.
    {
        Letterbox lb = Letterbox::compute({3000, 1080}, base, ScalingMode::FIT);
        check(!lb.contains({10.0f, 540.0f}, base),
              "a click in the side bar is reported as outside the content");
        check(lb.contains({1500.0f, 540.0f}, base),
              "a click in the middle is reported as inside the content");
    }

    // Degenerate inputs must not divide by zero.
    {
        Letterbox zero = Letterbox::compute({0, 0}, base, ScalingMode::FIT);
        sf::Vector2f const p = zero.to_base({100.0f, 100.0f});
        check(std::isfinite(p.x) && std::isfinite(p.y),
              "a zero-sized window does not divide by zero");
    }
}

// --------------------------------------------------------------- Camera

void test_camera()
{
    section("Camera shake");

    GameTime::reset();

    Camera camera({960.0f, 540.0f}, {480.0f, 270.0f});
    camera.set_max_shake_offset(24.0f);
    camera.set_trauma_decay(1.5f);

    check_near(camera.get_trauma(), 0.0f, 1e-5f, "starts with no trauma");

    // Trauma accumulates rather than being replaced.
    camera.shake(0.3f);
    camera.shake(0.3f);
    check_near(camera.get_trauma(), 0.6f, 1e-4f,
               "two shakes stack instead of overwriting");

    // ...but cannot run away.
    for(int i = 0; i < 20; ++i) camera.shake(0.5f);
    check_near(camera.get_trauma(), 1.0f, 1e-4f, "trauma is capped at 1");

    // And it decays.
    for(int i = 0; i < 30; ++i) camera.update(sf::seconds(1.0f / 60.0f));
    check(camera.get_trauma() < 0.3f, "trauma decays over time");

    /*
     * Whole-pixel offsets.
     *
     * A fractional offset moves glyphs and textures, which resample,
     * but not solid shapes, which still cover the same pixels. The
     * result looked like only the text was shaking.
     */
    {
        Camera snapped({960.0f, 540.0f}, {480.0f, 270.0f});
        snapped.set_max_shake_offset(24.0f);

        bool all_integral = true;
        for(int i = 0; i < 200; ++i)
        {
            snapped.shake(0.4f);
            snapped.update(sf::seconds(1.0f / 60.0f));

            sf::Vector2f const offset = snapped.get_shake_offset();
            if(std::fabs(offset.x - std::round(offset.x)) > 1e-4f) all_integral = false;
            if(std::fabs(offset.y - std::round(offset.y)) > 1e-4f) all_integral = false;
        }
        check(all_integral,
              "regression: shake offset is always a whole number of pixels");
    }

    // The UI view carries the shake but not the world position.
    {
        Camera ui_cam({960.0f, 540.0f}, {5000.0f, 4000.0f});   // far from origin
        ui_cam.set_max_shake_offset(20.0f);
        ui_cam.shake(1.0f);
        ui_cam.update(sf::seconds(1.0f / 60.0f));

        sf::Vector2f const ui_center    = ui_cam.get_ui_view().getCenter();
        sf::Vector2f const world_center = ui_cam.get_view().getCenter();

        check(std::fabs(ui_center.x - 480.0f) <= 20.0f &&
              std::fabs(ui_center.y - 270.0f) <= 20.0f,
              "ui view stays centred on the screen, not the world");
        check(world_center.x > 4000.0f,
              "world view is still out at the camera's world position");

        sf::Vector2f const shake = ui_cam.get_shake_offset();
        check_near(ui_center.x - 480.0f, shake.x, 1e-3f,
                   "ui view carries exactly the shake offset");
    }

    /*
     * Sustained fire versus a one-off explosion.
     *
     * Holding the trigger should settle at a moderate, steady level,
     * and a big hit on top of it should spike and then come back down
     * to that level -- not to zero, and not stay pinned at maximum.
     *
     * Impulses alone cannot express this. Trauma decays linearly, so a
     * stream of small impulses either dies between shots or, if they
     * arrive faster than the decay, saturates at the cap and never
     * recovers. That saturation is exactly what made an explosion
     * during sustained fire never settle back down.
     */
    {
        Camera gun({960.0f, 540.0f}, {480.0f, 270.0f});
        gun.set_trauma_decay(1.9f);
        gun.set_sustained_decay(5.0f);

        sf::Time const dt = sf::seconds(1.0f / 60.0f);

        // Hold the trigger: refresh the floor every frame.
        for(int i = 0; i < 120; ++i)
        {
            gun.set_sustained_shake(0.26f);
            gun.update(dt);
        }

        float const sustained = gun.get_trauma();
        check_near(sustained, 0.26f, 0.03f,
                   "regression: held fire settles at a steady mid level, not the cap");

        // Explosion on top, while still firing.
        gun.shake(0.7f);
        gun.set_sustained_shake(0.26f);
        gun.update(dt);

        check(gun.get_trauma() > sustained + 0.4f,
              "an explosion spikes well above the sustained level");

        // Keep firing; the spike should bleed off back to the floor.
        for(int i = 0; i < 90; ++i)
        {
            gun.set_sustained_shake(0.26f);
            gun.update(dt);
        }

        check_near(gun.get_trauma(), sustained, 0.05f,
                   "regression: it settles back to the rumble, not stuck at max");

        // Release the trigger: the floor lets go.
        for(int i = 0; i < 60; ++i) gun.update(dt);
        check_near(gun.get_trauma(), 0.0f, 1e-4f,
                   "releasing the trigger drops the sustained shake to nothing");
    }

    GameTime::reset();
}

// --------------------------------------------------------------- Random

void test_random()
{
    section("Random");

    Random::seed(12345);
    std::vector<int> first;
    for(int i = 0; i < 20; ++i) first.push_back(Random::integer(0, 1000));

    Random::seed(12345);
    std::vector<int> second;
    for(int i = 0; i < 20; ++i) second.push_back(Random::integer(0, 1000));

    check(first == second, "seeding produces a reproducible sequence");

    bool in_range = true;
    for(int i = 0; i < 1000; ++i)
    {
        float const v = Random::real(-5.0f, 5.0f);
        if(v < -5.0f || v > 5.0f) in_range = false;
    }
    check(in_range, "Random::real stays inside its bounds");

    // Degenerate ranges must not throw or loop.
    check(Random::integer(7, 7) == 7, "an empty integer range returns the bound");
    check_near(Random::real(2.0f, 2.0f), 2.0f, 1e-6f, "an empty real range returns the bound");
}

// ----------------------------------------------------------------- main

int main()
{
    std::printf("\n\033[1mSFML Framework test suite\033[0m\n");

    test_easing();
    test_math();
    test_game_time();
    test_event_bus();
    test_letterbox();
    test_camera();
    test_particles();
    test_markov();
    test_random();

    std::printf("\n%d checks, \033[%sm%d failures\033[0m\n\n",
                g_checks, g_failures == 0 ? "32" : "31", g_failures);

    return g_failures == 0 ? 0 : 1;
}
