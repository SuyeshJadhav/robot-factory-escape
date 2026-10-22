# Robot Factory Escape — Milestone 1

A single-player platformer built with the team's GEF engine. Climb the platforms,
avoid the patrol drone, and reach the cyan exit.

## Requirements

- Git, CMake 3.28+, Ninja, and a C++20 compiler (GCC or Clang).
- SDL3's system development libraries; see the
  [engine's platform setup instructions](https://github.com/ShounakDeshmukh/GEF_engine/blob/835c7804b8f3ce236330a9d16231f5e5a07500ac/README.md#required-toolchain).
- Internet access during the first configuration.
- A graphical desktop to play the game.

## Setup

Obtain **only the game**. Once the corrected `milestone1` branch is published:

```sh
git clone --branch milestone1 --single-branch https://github.com/SuyeshJadhav/robot-factory-escape.git
cd robot-factory-escape
```

Alternatively, extract the game submission archive or
[download the game ZIP](https://github.com/SuyeshJadhav/robot-factory-escape/archive/refs/heads/milestone1.zip)
and open a terminal in the extracted game directory. Git is still required for
CMake's dependency downloads.

## Build and run

From the game directory:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
./build/robot_factory_escape
```

CMake automatically downloads engine **v0.1.0**, pinned to commit
`835c7804b8f3ce236330a9d16231f5e5a07500ac`, and its third-party dependencies into
`build/_deps/`. No separate engine clone or sibling folder is needed; an existing
sibling engine is ignored by default. The initial configuration and build may
take several minutes.

**Optional local engine:** add `-DGEF_ENGINE_DIR=/absolute/path/to/GEF_engine` to
the configure command to use an existing v0.1.0 checkout instead. Keep its complete
example assets, including `examples/hello_entity/assets/SuperBouncer.ttf`.

## Controls

| Key | Action |
| --- | --- |
| A / D | Move left / right |
| Space | Jump while grounded |
| P | Toggle display scaling |
| R | Restart |
| Close window | Quit |

## Tests

After building:

```sh
ctest --test-dir build --output-on-failure
```

The five checks cover build setup, player movement, drone patrol, game progress,
and sprites. Setup checks run offline with configuration-only fixtures; the
sprite check uses a headless renderer automatically.

## Troubleshooting

| Problem | Action |
| --- | --- |
| GitHub download fails | Check internet access and Git installation, then rerun configuration. |
| Local engine/font not found | Correct the optional local path, or pass `-DGEF_ENGINE_DIR=` to restore automatic downloading. |
| Older build uses the wrong engine | Use a fresh build directory or clear the old override with `-DGEF_ENGINE_DIR=`. |
| `No available video device` | Start the game from a graphical desktop, not a headless terminal. |
| Image/font cannot be opened | Keep the game assets and build directory in place. Reconfigure and rebuild after moving the checkout; copying only the executable is not supported. |

For a Linux headless startup check only:

```sh
SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software timeout 8s ./build/robot_factory_escape
```

`Factory ready` confirms startup; exit 124 means the timeout stopped the game.
This does not open a playable window or verify keyboard input.

Verified on Linux with a fresh online configuration, no sibling engine or local
source overrides: engine v0.1.0 downloaded, build succeeded, all five tests
passed, and headless startup reached `Factory ready`. Other operating systems
and full interactive gameplay have not been verified.
