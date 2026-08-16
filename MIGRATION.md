# Migration Guide

Every API change in this pass, and what to replace it with. Work top to
bottom, the asset and event-handling changes touch the most call sites.

---

## 1. Assets are borrowed, not copied

The biggest change. Widgets no longer copy the fonts and textures you
hand them; they store a pointer. **Anything you pass must outlive the
widget.** Passing a stack local now dangles where it used to silently
deep-copy.

Load through the shared cache, which lives until the program exits:

```cpp
#include "core/AssetCache.hpp"

const sf::Font*    font = Assets::font("assets/fonts/main.ttf");
const sf::Texture* tex  = Assets::texture("assets/ui.png");
```

Both return `nullptr` and log on failure rather than throwing, and both
disable smoothing, see §E for why that matters for text sharpness.

**Before, this used to work, and now dangles:**

```cpp
void Game::initialize_objects()
{
    sf::Font font("assets/fonts/Windows-Regular.ttf");   // stack local
    sf::Texture texture("assets/textures/simple_button.png");
    button->add_text("Click Me!", font, sf::Color::Black);
    button->set_background_texture(texture);
}   // font and texture destroyed here
```

**After:**

```cpp
void DemoScene::build_widgets()
{
    const sf::Font* font = Assets::font("assets/fonts/main.ttf");
    if(!font) return;

    button->add_text("Click Me!", *font, sf::Color::Black);

    if(const sf::Texture* tex = Assets::texture("assets/ui.png"))
        button->set_background_texture(*tex);
}
```

### Changed signatures

| Before | After |
|---|---|
| `add_text(std::string, sf::Font&, sf::Color)` | `add_text(const std::string&, const sf::Font&, sf::Color)` |
| `set_background_texture(sf::Texture, bool)` | `set_background_texture(const sf::Texture&, bool)` |
| `Slider::set_thumb_texture(sf::Texture, bool)` | `set_thumb_texture(const sf::Texture&, bool)` |
| `Slider::set_body_texture(sf::Texture, bool)` | `set_body_texture(const sf::Texture&, bool)` |
| `Gauge::set_dial_texture(sf::Texture, bool)` | `set_dial_texture(const sf::Texture&, bool)` |
| `Gauge::set_arm_texture(sf::Texture, bool)` | `set_arm_texture(const sf::Texture&, bool)` |
| `Frame(sf::Vector2f, sf::Color, sf::Font&)` | `Frame(sf::Vector2f, sf::Color, const sf::Font&)` |
| `Frame::add_taskbar_exit(sf::Texture, bool)` | `Frame::add_taskbar_exit_texture(const sf::Texture&, bool)` |
| `TextBox(..., sf::Font&, ...)` | `TextBox(..., const sf::Font&, ...)` |

---

## 2. Widget event handling

`handle_event` now receives the actual `sf::Event`. The `RenderWindow`
parameter is gone (nothing used it) and the optional local-position
parameter is gone (containers convert coordinates themselves).

```diff
- void handle_event(const sf::RenderWindow& window,
-                   sf::Vector2f world_pos,
-                   std::optional<sf::Vector2f> local_mouse_pos = std::nullopt);
+ void handle_event(const sf::Event& event, sf::Vector2f mouse_pos);
```

Dispatch once per event, and call `begin_frame()` once per frame before
any events:

```cpp
void MyScene::begin_frame()          { m_widgets.begin_frame(); }
void MyScene::handle_event(const sf::Event& e, sf::Vector2f mouse)
{
    m_widgets.handle_event(e, mouse);
}
```

`SceneManager` calls `Scene::begin_frame()` for you.

### Button click semantics changed

`is_pressed()` used to be true on every frame the mouse was held, and
the callback fired once per queued event per frame. Now:

| Method | Meaning |
|---|---|
| `is_hovered()` | cursor is over the button |
| `is_pressed()` | mouse went down on it and is still down |
| `was_clicked()` | **new**, a full press-and-release completed this frame |

Callbacks fire exactly once per completed click. If you were using
`is_pressed()` to detect clicks, switch to `was_clicked()` or a callback:

```diff
- if(button->is_pressed()) { fire_once(); }
+ if(button->was_clicked()) { fire_once(); }
```

Dragging off the button before releasing now cancels the click.

---

## 3. Read keys from the event, not the keyboard

```diff
  if(const auto* key = event.getIf<sf::Event::KeyPressed>())
  {
-     if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter)) { ... }
+     if(key->code == sf::Keyboard::Key::Enter) { ... }
  }
```

The old form asked "is Enter down right now", so holding Enter and
pressing any other key triggered the Enter branch.

For **held** input, movement, thrust — poll in `update()` instead, once
per frame:

```cpp
void GameScene::update(sf::Time dt)
{
    if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W))
        m_ship.thrust(dt);
}
```

---

## 4. Time and tweening

`Interpolated<T>` now reads `GameTime` instead of `steady_clock`.
`SceneManager` advances it once per fixed step, if you write your own
loop, you must call `GameTime::advance(dt)` or nothing will animate.

New capabilities:

```cpp
GameTime::pause();              // freezes every tween in the game
GameTime::set_scale(0.25f);     // slow motion
GameTime::resume();

my_tween.set_unscaled(true);    // opt out; keeps running while paused
```

`Interpolated::operator=` now returns `Interpolated&` rather than
`void`, so chained assignment compiles.

---

## 5. Camera

Shake was rebuilt around accumulating trauma, so `shake()` takes a
different kind of argument.

```diff
- m_camera.shake(50);        // pixels, replaced any shake in progress
+ m_camera.shake(0.35f);     // trauma 0-1, adds to whatever is running
```

Rough equivalents: `0.1` a tap, `0.25` a solid hit, `0.5` an explosion,
`1.0` maximum. Tune `set_max_shake_offset()` for the pixel amplitude.

`update()` now needs the delta time:

```diff
- m_camera.update();
+ m_camera.update(delta_time);
```

Removed: `shake(int, float degrees)`, directional shake. Trauma shake
is omnidirectional. Removed: `Camera::move(sf::Vector2f, bool)`.

Renamed: `get_center()` returns the shaken centre; use
`get_target_center()` for the un-shaken position when gameplay logic
needs a stable value.

New: `follow(target, dt, smoothing)`, `set_bounds()`, `set_zoom()`,
`get_viewport_bounds()`.

---

## 6. Particles

**Speeds are now per second, not per frame.** Multiply your old tuned
values by your framerate, a magnitude of `3` at 60fps becomes `180`.

```diff
- ps->add_particles(30, color, Vector{4, sf::degrees(0)}, 180);
+ ps->add_particles(30, color, Vector{240.f, sf::degrees(0.f)}, 180.f);
```

The last parameter is now `float spread_degrees` rather than
`int randomness`, and `0` is legal (it used to divide by zero).

**Drag is per second.** The old default of `0.99` per frame works out to
about `0.55` per second at 60fps:

```diff
- ps.set_drag(0.92f);    // per frame
+ ps.set_drag(0.006f);   // per second, 0.92^60
```

**Gravity is a Cartesian vector** in units/sec², positive y is down:

```diff
- ps.set_gravity(Vector{0.05f, sf::degrees(270)});
+ ps.set_gravity(sf::Vector2f{0.f, 200.f});
```

### ParticleSystemManager

`sync_with_view()` is **gone**, along with the whole shifting scheme.
Particles are in world space; draw them through your camera view:

```diff
- m_psm.sync_with_view(m_camera.get_position());
- m_whole_screen_texture.draw(m_psm, m_shaders.get_shader("bloom").get());
+ target.setView(m_camera.get_view());
+ target.draw(m_psm);
```

`set_particle_system_position()` now takes plain world coordinates.
`get_global_particle_positions()` is renamed `get_particle_positions()`
and no longer applies a correction term.

`get_particles()` returns `const std::vector<Particle>&` rather than a
copy.

---

## 7. Bloom moved to a post-process

The old single-pass `bloom.frag` is replaced by `bloom_extract.frag` +
`bloom_blur_v.frag`, driven by `BloomEffect` and applied to the finished
frame rather than to the particle layer.

```diff
- sf::Shader bloom;
- bloom.loadFromFile("shaders/bloom.frag", sf::Shader::Type::Fragment);
- bloom.setUniform("threshold", 0.1f);
- m_shader_manager.add_shader("bloom", std::move(bloom));
+ sm.enable_bloom("shaders");
+ sm.get_bloom().set_threshold(0.55f);
+ sm.get_bloom().set_intensity(1.4f);
```

Thresholds do not carry over. The old shader double-counted the centre
tap, so a low threshold like `0.1` was needed to compensate; start
around `0.5` now.

### ShaderManager

```diff
- void add_shader(std::string key, sf::Shader shader);
- std::shared_ptr<sf::Shader> get_shader(std::string key);
+ sf::Shader* load_fragment(const std::string& key, const std::string& path);
+ sf::Shader* load(const std::string& key, const std::string& vert, const std::string& frag);
+ sf::Shader* get_shader(const std::string& key) const;   // nullptr if absent
```

`get_shader()` no longer inserts an empty entry on a miss, and returns a
raw pointer, which is what `RenderTarget::draw` wants anyway.

---

## 8. Scenes

`Scene` no longer holds raw pointers to the window, render texture and
sprite. The manager passes what you need into each call.

```diff
  class MyScene : public Scene {
-     void process_events() override;
-     void update(sf::Time dt) override;
-     void render() override;
+     void begin_frame() override;
+     void handle_event(const sf::Event& e, sf::Vector2f mouse_pos) override;
+     void update(sf::Time dt) override;
+     void render(sf::RenderTarget& target) override;
+     sf::Color get_clear_color() const override;
  };
```

`SceneManager` polls events, maps the mouse into base-resolution space
and clears the target for you. Inside `render()` just draw; do not call
`display()` or touch the window.

For world-space mouse coordinates:

```cpp
sf::Vector2f world = m_manager->map_to_world(mouse_pos, m_camera.get_view());
```

### SceneManager

```diff
- SceneManager sm;
- sm.run_scene(60);
+ SceneManager sm("Void Drift");        // window title; add `true` for fullscreen
+ sm.run(60);
```

`swap_scene()` with `FADE` no longer blocks. It used to run its own
inner loop that never polled events, freezing the window for the whole
transition. It is now a state machine advanced by the main loop.

Scene keys bound to `EventType` values, unchanged from your original
intent:

| Event | Scene key |
|---|---|
| `SWITCH_TO_MAIN_MENU` | `"menu"` |
| `SWITCH_TO_GAME` | `"game"` |
| `SWITCH_TO_SETTINGS` | `"settings"` |
| `SWITCH_TO_GAME_OVER` | `"gameover"` |
| `SWITCH_TO_PERK_SELECT` | `"perks"` |

`QUIT`, `PAUSE`, `RESUME` and `TOGGLE_PAUSE` are also handled.

---

## 9. EventBus

Prefer `subscribe_scoped`, which unsubscribes in its destructor:

```diff
- m_pause_handle = EventBus::subscribe(EventType::TOGGLE_PAUSE,
-     [this](const Event& e) { ... });
- // ...and never unsubscribed, leaving a dangling `this`
+ Subscription m_pause_sub;   // member
+ m_pause_sub = subscribe_scoped(EventType::TOGGLE_PAUSE,
+     [this](const Event& e) { ... });
```

`Event` gained a `float m_value` and both extra fields are defaulted, so
`Event(EventType::QUIT)` compiles.

---

## 10. Renamed and removed

| Old | New |
|---|---|
| `enum Alignment { CENTER, LEFT, RIGHT }` | `enum class Alignment`, qualify as `Alignment::CENTER` |
| `Widget::get_position()` etc. | now `const` |
| `WidgetManager::get_widgets()` | returns `const&` |
| `Slider::get_percentage()` | now `const`; added `set_percentage()`, `get_value(min,max)` |
| `Gauge::set_arm_angle_based_on_value(v)` | `set_value(v, instant)` |
| `Gauge::set_arm_angle_based_on_degrees(d)` | `set_arm_degrees(d, instant)` |
| `TextBox::add_text_scroll(int speed)` | `add_text_scroll(float chars_per_second)` |
| `Markov::update_base_names()` | returns `bool` |
| `random_gen`, `rd` globals | `Random::generator()`, or the `Random::` helpers |
| `WINDOW_WIDTH` etc. mutable | now `constexpr` |
| `Game` class | gone, use `SceneManager` + a `Scene` |
| `ParticleSystem::get_particle_lines()` | returns `const&` |

`Gauge` value mapping was also **fixed**: it now honours the lower ends
of both the range and the boundary. If you compensated for the old
behaviour by offsetting your inputs, remove that compensation.

---

## 11. Build

```
make            # incremental, header-aware
make debug      # -O0 -g with ASan + UBSan
make release    # -O2 -DNDEBUG
make test       # logic tests, no window needed
make run
make info       # show detected platform and flags
```

Objects now go to `build/` instead of sitting next to sources.
`-MMD -MP` means editing a header rebuilds what depends on it, without
this, your `SceneManager.o` was built from source that no longer existed.

macOS picks up Homebrew automatically. On Windows with MSYS2/MinGW:

```
mingw32-make SFML_ROOT=C:/SFML-3.0.0
mingw32-make release WINDOWED=1     # no console window
```

---

# Round 2, fixes from your first run

Six issues, and what changed for each.

## A. Screen shake now reaches the HUD (optional)

Shake lives in the camera view, so previously only things drawn through
that view moved, the world layer shook under a rock-steady HUD.

`Camera` now exposes the shake on its own:

```cpp
target.setView(m_camera.get_view());       // world: pans AND shakes
target.draw(m_particles);

target.setView(m_camera.get_ui_view());    // HUD: shakes, does not pan
target.draw(m_widgets);

target.setView(target.getDefaultView());   // HUD: welded to the glass
target.draw(m_widgets);
```

`get_ui_view()` is screen space plus the shake offset. That's the one
you want for the tank cannon, the whole display jolts, but the HUD
stays where you put it when the camera pans. In the demo, `U` toggles
between the two so you can compare.

Also added: `get_shake_offset()`, `get_shake_roll()`, and
`set_shake_roll_enabled()`.

**Roll is now off by default.** The rotational component looked good but
rotates the whole view, which pushes screen-space hit testing further
off the further you are from the centre. Turn it back on with
`set_shake_roll_enabled(true)` if you want it.

## B. Mouse coordinates, fixed

The cause: `SceneManager` divided the cursor position by a hard-coded
`SCALE` of 2. That's only correct if the window is exactly 1920×1080.
Ask for a 1920×1080 window on a 1920×1080 desktop and the window manager
shrinks it to make room for the title bar, so SFML stretched 1080 rows
of content into ~1000 physical rows. The error was zero at the top edge
and grew linearly downward, exactly the "worse the lower you click"
symptom.

Three changes:

1. **The window now fits the desktop.** `SceneManager` picks the largest
   whole-number upscale that actually fits, instead of always demanding
   `WINDOW × SCALE`.
2. **The mapping is computed, not assumed.** New `Letterbox` type
   (`core/Letterbox.hpp`) works out scale and offset from the real window
   size, and mouse input is inverted through exactly the transform used
   to present the frame. `sf::Event::Resized` recomputes it.
3. **Aim uses the settled camera.** The demo maps clicks through the
   camera's target centre, not the live shaking one, otherwise a click
   during a shake lands wherever the camera happened to be displaced to.

There are 18 tests covering this across eight window sizes, including
the 1920×1000 case.

```cpp
const Letterbox& lb = sm.get_presentation();
sf::Vector2f base = lb.to_base({pixel_x, pixel_y});
bool on_content   = lb.contains({pixel_x, pixel_y}, BASE_RESOLUTION);
```

**Scaling is fractional by default now.** Integer-only scaling sounds
right for pixel art, but a 1920×1000 window drops straight from 2× to
1× and renders the game at a quarter of the available area. Since the
render target has smoothing off, fractional upscaling is
nearest-neighbour, uneven pixel sizes, never blurry. Opt back in with
`sm.set_integer_scaling(true)`.

## C. Bloom dialled back, and on a slider

New defaults are much more restrained:

| | Before | Now |
|---|---|---|
| threshold | 0.55 | **0.80** |
| soft knee | 0.25 | **0.15** |
| intensity | 1.4 | **0.45** |

```cpp
sm.get_bloom().set_intensity(0.45f);   // 0 = off, ~1.5 = heavy
sm.get_bloom().set_threshold(0.80f);   // raise to bloom only the brightest
```

The demo's slider now drives intensity live with a readout above it.
Note `Slider::set_percentage()` deliberately does *not* fire the
callback, so push the initial value through by hand once at startup —
the demo does this in `on_enter()`.

## D. Pause no longer makes tweens race to catch up

`Scene::update()` now receives a **scaled** delta time. While paused it
is zero, so a scene that accumulates `dt` freezes along with everything
else.

That was the bug: the demo advanced its own phase counter with raw `dt`
while paused, so the gauge's *target* kept moving while the tween
driving it was frozen. On unpause the arm found its target far away and
sprinted after it.

For anything that must keep running while paused, override the new
sibling:

```cpp
void update(sf::Time dt) override          { /* gameplay, pauses */ }
void update_realtime(sf::Time dt) override { /* pause menu, never pauses */ }
```

## E. Text sharpness restored

`sf::Font` smooths its glyph atlas by default. At 12px on a target
that's then scaled up, that's what turned everything to mush, the old
code called `font.setSmooth(false)` and I dropped it.

**Use `Assets::font()` rather than `Assets::fonts().get()`.** It loads
through the same cache and disables smoothing:

```cpp
const sf::Font* font = Assets::font("assets/fonts/main.ttf");   // crisp
const sf::Font* big  = Assets::font("assets/fonts/title.ttf", true);  // smoothed
```

`Assets::texture()` does the same for textures. Demo character sizes are
back to 12 throughout.

One thing worth knowing: the bundled `default.ttf` is DejaVu Sans, a
vector font, fine, but a real pixel font at its native size will look
considerably better. Point `FONT_PATH` in `DemoScene.cpp` at your
original `Windows-Regular.ttf`.

## F. HUD panning, see A

`get_ui_view()` covers exactly the case you described. Three options now:
pan and shake (`get_view()`), shake only (`get_ui_view()`), or neither
(`getDefaultView()`).

---

# Round 3, fixes from the second run

## G. Gentle shakes moved only the text

Sub-pixel problem. A shake offset of, say, 0.4px doesn't change which
pixels a solid rectangle covers, but text and textures are sampled from
an atlas, so they resample and visibly shift. The result was a light
shake where only the lettering appeared to wobble.

The camera now rounds both the shake offset and the settled position to
whole pixels, so everything moves together or not at all:

```cpp
m_camera.set_pixel_snap(false);   // opt out if you're not doing pixel art
```

## H. Black bars, gone by default

Those were my letterboxing. Scaling now has three modes:

| Mode | Behaviour |
|---|---|
| `STRETCH` | **default.** Fills the window on both axes. No bars ever. Distortion equals the aspect difference, 2-4% in a maximised window, invisible. |
| `FIT` | Preserves aspect exactly, centred, bars on one axis. |
| `INTEGER` | `FIT` snapped to a whole-number scale. Perfect pixel grid, biggest bars. |

```cpp
sm.set_scaling_mode(ScalingMode::FIT);   // if you'd rather have bars than skew
```

`Letterbox::scale` is now an `sf::Vector2f` rather than a float, since
`STRETCH` scales the axes independently. `set_integer_scaling(bool)` is
replaced by `set_scaling_mode(ScalingMode)`.

## I. Sustained fire pinning the shake at maximum

The real one. Trauma decays **linearly**, which means a stream of small
impulses has no stable middle ground: either they arrive slower than the
decay and die between shots, or faster and the level saturates at the
cap. Holding fire saturated it, so an explosion on top had nowhere to go
and nothing to decay back to, exactly what you described.

Continuous sources now use a separate channel:

```cpp
if(firing)    camera.set_sustained_shake(0.26f);   // every frame it's active
if(explosion) camera.shake(0.8f);                  // once
```

The floor holds at exactly what you ask for while you keep asking, and
falls away on its own when you stop, no "stop" call needed. Impulses
ride on top, so an explosion spikes above the rumble and settles back
down *to the rumble* rather than to zero or staying pinned.

`get_trauma()` now returns the effective level, floor included.
`set_sustained_decay()` controls how fast the floor releases.

Related: `SceneManager` now calls `setKeyRepeatEnabled(false)`. With key
repeat on, holding a key fires its action at the OS repeat rate, so
anything accumulating per keypress runs away. Poll held keys in
`update()` instead. Turn it back on with `set_key_repeat_enabled(true)`
if you add text input.

## J. Particles vanishing on pause

Each particle draws as a line from its previous position to its current
one. Stepping with `dt = 0` set previous = current, collapsing every
line to zero length, so the instant you paused, they all disappeared.

`ParticleSystem::update()` now returns early on a non-positive step,
leaving the last frame's vertex data intact. Particles stay on screen,
frozen, which is what pause should look like.

## K. Enter also pausing

That was leftover demo code, Enter both toggled the taskbar and
published `TOGGLE_PAUSE`. Gone with the old demo.
