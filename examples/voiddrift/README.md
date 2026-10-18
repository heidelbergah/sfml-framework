# Void Drift — quarantined

These are the four scenes from your Void Drift game, moved out of the
framework tree untouched. **They do not currently compile**, and they
are deliberately excluded from the Makefile's source list.

## Why they're here

They were sitting in `include/scenes/` and `src/scenes/` alongside the
framework's own `Scene` and `SceneManager`. Since they're game code
rather than framework code, they've been separated out — the framework
now builds and runs without them.

## What's missing

Six headers these scenes include were not in the archive:

- `Ship.hpp`
- `Asteroid.hpp`
- `Bullet.hpp`
- `ScorePickup.hpp`
- `Starfield.hpp`
- `RunState.hpp`

Drop your originals into `examples/voiddrift/include/` and adjust the
include paths at the top of each scene.

## What needs updating

These files were written against the old API. Work through
[`MIGRATION.md`](../../MIGRATION.md) — the sections that will bite are:

1. **Scene interface** (§8) — `process_events()` / `render()` became
   `handle_event(event, mouse_pos)` / `render(target)`. The raw
   `m_render_window` / `m_render_texture` / `m_render_sprite` pointers
   are gone; the manager passes the target in.
2. **Assets** (§1) — `sf::Font m_font` members must become
   `const sf::Font*` from `Assets::fonts()`.
3. **HUD text** — these scenes hold raw `sf::Text*` members
   (`m_score_text`, `m_lives_text`, `m_level_text`). Prefer `TextBox`
   widgets, or `std::unique_ptr<sf::Text>` if you want the raw type.
4. **Particles** (§6) — speeds are per second now; multiply your old
   magnitudes by 60.
5. **Camera** (§5) — `shake(50)` becomes `shake(0.5f)`, and `update()`
   takes a delta time.

## Reconnecting them

Once the entity headers are back and the scenes are migrated, add them
to the build by appending to `SRC_DIRS` in the Makefile:

```make
SRC_DIRS := src src/core src/framework src/gui src/scenes src/utilities \
            examples/voiddrift/src
```

and drop `examples/demo` from that list so there's a single `main()`.
Your original entry point works unchanged apart from the window title:

```cpp
SceneManager sm("Void Drift");
sm.add_scene("menu",     std::make_shared<MenuScene>());
sm.add_scene("game",     std::make_shared<GameScene>());
sm.add_scene("perks",    std::make_shared<PerkScene>());
sm.add_scene("gameover", std::make_shared<GameOverScene>());
sm.swap_scene("menu", SceneTransitionType::INSTANT);
sm.run(60);
```

The scene keys `"menu"`, `"game"`, `"perks"` and `"gameover"` are
already wired to `SWITCH_TO_MAIN_MENU`, `SWITCH_TO_GAME`,
`SWITCH_TO_PERK_SELECT` and `SWITCH_TO_GAME_OVER`, so the event-driven
transitions your scenes publish will work as written.
