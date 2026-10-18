# SFML 3 Framework

A 2D game framework built on SFML 3.0, scenes, GUI widgets, particles,
camera effects, shaders and tweening, organised so a game can be built
on top rather than around it.

```
make && make run
```

Requires SFML 3.0 and a C++20 compiler. See [Building](#building).

---

## What's here

```
include/
  core/         GameTime, Interpolated, AssetCache, Easing, Math, Random, Config
  framework/    Camera, EventBus, Particle(System)(Manager), Shader/BloomEffect
  gui/          Widget, TextWidget, Button, TextBox, Frame, Slider, Gauge, WidgetManager
  scenes/       Scene, SceneManager
  utilities/    Markov name generator
src/            implementations, mirroring include/
examples/
  tetris/       Cascade, a full Tetris clone (built by default)
shaders/        bloom_extract.frag, bloom_blur_v.frag
tests/          logic tests, no window required
```

---

## The five things worth knowing

### 1. SceneManager owns the loop

It creates the window, runs a fixed timestep, advances `GameTime`,
handles transitions and presents the frame. A `Scene` only says what
happens inside it.

```cpp
#include "scenes/SceneManager.hpp"

int main()
{
    SceneManager sm("Void Drift");
    sm.enable_bloom("shaders");

    sm.add_scene("menu",     std::make_shared<MenuScene>());
    sm.add_scene("game",     std::make_shared<GameScene>());
    sm.add_scene("perks",    std::make_shared<PerkScene>());
    sm.add_scene("gameover", std::make_shared<GameOverScene>());

    sm.swap_scene("menu", SceneTransitionType::INSTANT);
    sm.run(60);
}
```

A scene overrides what it needs:

```cpp
class GameScene : public Scene
{
    void begin_frame() override                 { m_widgets.begin_frame(); }
    void handle_event(const sf::Event& e, sf::Vector2f mouse) override;
    void update(sf::Time dt) override;
    void render(sf::RenderTarget& target) override;
    sf::Color get_clear_color() const override  { return sf::Color::Black; }
};
```

Switch scenes from anywhere by publishing an event:

```cpp
EventBus::publish(Event(EventType::SWITCH_TO_GAME_OVER));
```

`SWITCH_TO_MAIN_MENU`, `SWITCH_TO_GAME`, `SWITCH_TO_SETTINGS`,
`SWITCH_TO_GAME_OVER` and `SWITCH_TO_PERK_SELECT` map to the scene keys
`"menu"`, `"game"`, `"settings"`, `"gameover"` and `"perks"`.

### 2. Load assets through the cache

Fonts and textures are shared, never copied. Widgets borrow them.

```cpp
const sf::Font* font = Assets::font("assets/fonts/main.ttf");
if(font) button->add_text("Play", *font, sf::Color::Black);
```

Use `Assets::font()` and `Assets::texture()` rather than the raw caches:
they disable smoothing, which is what keeps small text and pixel art
sharp. `sf::Font` smooths its glyph atlas by default and it turns 12px
text to mush once the frame is scaled up.

The cache outlives every scene, and both return `nullptr` rather than
throwing so a missing asset degrades instead of crashing.

### 3. One clock drives everything

```cpp
GameTime::pause();            // every tween, particle and camera move freezes
GameTime::set_scale(0.2f);    // slow motion
GameTime::resume();
```

`SceneManager` advances it. Tweens that must survive a pause, pause
menus, UI, opt out with `set_unscaled(true)`.

### 4. Assign to a value and it tweens

```cpp
Interpolated<sf::Vector2f> pos;
pos.set_transition(TransitionFunction::EaseOutBack);
pos.set_duration(0.4f);

pos = {200.f, 100.f};          // starts the tween
sprite.setPosition(pos);       // implicitly converts to the current value
```

Twelve curves, including the `back`, `elastic` and `bounce` families.
Retargeting mid-flight starts from wherever the value currently is.

### 5. Camera shake stacks

```cpp
m_camera.shake(0.1f);    // a tap
m_camera.shake(0.5f);    // an explosion
m_camera.update(dt);
```

Shake is trauma that accumulates and decays, with the visible offset
scaling as trauma squared and sampled from smooth noise. Overlapping
sources combine instead of overwriting, so an explosion during
machine-gun fire reads correctly.

Impulses are for hits. Anything continuous, a held trigger, an engine,
a collapsing building, uses the sustained channel instead:

```cpp
if(firing) camera.set_sustained_shake(0.26f);   // every frame it's active
```

The floor holds while you keep asking and releases when you stop, and
impulses spike above it and settle back down *to it*. Trauma decays
linearly, so a stream of impulses has no stable middle: it either dies
between shots or saturates at the cap and never recovers.

Choose per layer what the shake reaches:

```cpp
target.setView(m_camera.get_view());       // world: pans and shakes
target.draw(m_particles);

target.setView(m_camera.get_ui_view());    // HUD: shakes, doesn't pan
target.draw(m_widgets);                    //  <- the tank-cannon option

target.setView(target.getDefaultView());   // HUD: ignores shake entirely
target.draw(m_widgets);
```

---

## Quick reference

**Widgets**, `Button`, `TextBox`, `Frame`, `Slider`, `Gauge`, held in a
`WidgetManager` that z-orders them.

```cpp
auto button = std::make_shared<Button>(sf::Vector2f{96, 48}, sf::Color::Blue);
button->add_text("Play", font, sf::Color::Black);
button->set_transition(TransitionFunction::EaseOutElastic);
button->set_callback([this]{ EventBus::publish(Event(EventType::SWITCH_TO_GAME)); });
m_widgets.add_widget("play", button);
```

Callbacks fire once per completed click. `was_clicked()` is true for the
single frame a click landed; `is_pressed()` is true while held.

`Frame` is a container, children live in its local coordinate space and
it can be dragged by an optional taskbar.

**Particles**, speeds are units per second, angles are degrees with 0
right and 90 up.

```cpp
ParticleSystem sparks({0.f, 0.f});
sparks.set_lifespan(sf::seconds(2.f));
sparks.set_drag(0.4f);                       // fraction of speed kept per second
sparks.set_gravity(sf::Vector2f{0.f, 260.f});
sparks.set_gravity_enabled(true);
sparks.set_fade(true);
m_particles.add_particle_system("sparks", std::move(sparks));

// count, colour, velocity, spread in degrees
system->add_particles(30, sf::Color::White, Vector{240.f, sf::degrees(90.f)}, 45.f);
```

Particles are world space, draw them through the camera view:

```cpp
target.setView(m_camera.get_view());
target.draw(m_particles);

target.setView(target.getDefaultView());   // UI in screen space
target.draw(m_widgets);
```

**Events**, store the subscription and it cleans up after itself:

```cpp
class MenuScene : public Scene {
    Subscription m_sub;
public:
    void on_enter() override {
        m_sub = subscribe_scoped(EventType::PLAYER_DIED,
            [this](const Event& e){ /* ... */ });
    }
};
```

**Names**, `Markov` generates names in the style of a corpus:

```cpp
Markov names("assets/names.txt", 3);
std::string name = names.generate_name(4, 12);   // min and max length
```

---

## Example: Cascade, Tetris

`examples/tetris`. Guideline rules: seven-bag, SRS with wall kicks,
hold, five-piece preview, ghost, hard drop, lock delay with move
resets, T-spins, back-to-back and combos. Gruvbox dark throughout.

```
make && make run
make test-tetris     # 63 rule tests, no window needed
```

Arrows or A/D move, up/X/Z rotate, down soft drops, space hard drops,
C or shift holds, P pauses, escape quits.

It was written without touching the framework, only scenes, assets and
the existing shaders. What it leans on:

| Feature | Where you see it |
|---|---|
| Scenes + fades | menu → play → game over, switched by publishing an `EventType` |
| Shake | a tap on lock, scaled with distance on hard drop, harder per line cleared |
| `get_ui_view()` | the whole playfield jolts as one, well, stack, panels and text together |
| Particles | lock dust, hard-drop trails, line-clear bursts, level-up shimmer |
| `Interpolated` | board rim glow, hold flash, piece fade-in, clear collapse |
| `GameTime` | P freezes the board while the backdrop keeps breathing; topping out drops into slow motion |
| Bloom | the line-clear flash, kept restrained so the palette stays gruvbox |

To build your own game, point `SRC_DIRS` in the Makefile at your
scenes instead of `examples/tetris`.

Note that a `tests.cpp` inside an example directory is filtered out of
the game build, so an example can carry its own test suite without
colliding with `main()`.

---

## Building

```
make            # incremental; rebuilds when a header changes
make debug      # -O0 -g with AddressSanitizer + UBSan
make release    # -O2 -DNDEBUG
make run
make test       # logic tests, no window, safe for CI
make info       # detected platform and link flags
make clean
```

One Makefile covers all three platforms.

| Platform | Notes |
|---|---|
| **Linux** | Install SFML 3.0 dev packages, or build from source. |
| **macOS** | `brew install sfml`, Homebrew's prefix is detected automatically on both Apple Silicon and Intel. |
| **Windows** | MSYS2/MinGW: `mingw32-make SFML_ROOT=C:/SFML-3.0.0`. Add `WINDOWED=1` to a release build to drop the console window. |

---

## Roadmap

Done:

- [x] Particles and particle managers
- [x] Interpolated template for smooth transitional movement
- [x] GUI elements with uploadable textures
- [x] Frame element with the new widget handling
- [x] Shader support + shader manager
- [x] Camera effects: shake, movement
- [x] Event system (EventBus)
- [x] Scene management system
- [x] Pause, `GameTime::pause()` freezes every tween at once
- [x] Asset manager (fonts, textures, sound buffers)
- [x] Bloom as a separable post-process
- [x] Resolution-independent presentation with correct mouse mapping

Next:

- [ ] Sprite + animation system, spritesheet slicing, frame timing,
      animation states. Arguably more fundamental to a 2D game than the
      particle system already here.
- [ ] Collision primitives, AABB and circle tests, plus a spatial hash
      for broad phase. Naive n² is fine for twenty asteroids and falls
      over at two hundred.
- [ ] Input abstraction, map `"jump"` to a key or pad button rather
      than hardcoding `Key::Space`. Buys rebinding and gamepad support;
      painful to retrofit later.
- [ ] Tilemap manager
  - [ ] WFC map generation
  - [ ] JSON load/save
- [ ] Sound/music manager (`Assets::sounds()` already caches buffers)
- [ ] Cutscene support, with letterbox bars
- [ ] Entity base or light ECS, parallel `std::vector<Asteroid>`,
      `std::vector<Bullet>` lists stop scaling around the fourth type
- [ ] State machine helper for entity AI

Known limitations:

- `Frame` does not clip children to its bounds. A child positioned
  outside the frame draws outside it. Clipping needs a render texture,
  which is worth adding for `Frame` specifically once something needs it.
- Shake is omnidirectional; there is no directional variant.
