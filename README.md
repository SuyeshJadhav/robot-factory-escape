# Robot Factory Escape

A cyberpunk-themed platformer built with the team's GEF engine. Escape the robot
factory by navigating platforms, avoiding the patrol drone, and reaching the
raised exit. Play offline or connect multiple game processes using client-server
or hybrid peer-to-peer networking.

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=plastic&logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![SDL3](https://img.shields.io/badge/SDL-3-00599C?style=plastic&logo=sdl&logoColor=white)](https://www.libsdl.org/)
[![ZeroMQ](https://img.shields.io/badge/ZeroMQ-333333?style=plastic&logo=zeromq&logoColor=white)](https://zeromq.org/)
[![CMake](https://img.shields.io/badge/CMake-064F8C?style=plastic&logo=cmake&logoColor=white)](https://cmake.org/)

<a id="overview"></a>
## 🎮 Overview

The game consumes a sibling `GEF_engine` checkout rather than vendoring its own
engine or duplicating the networking transport. Milestone 3 validates the game
against engine **0.3.0**, revision
`0ec1cba862c91d2ed19bf2cf9b890cd51a5dede3`.

- Offline, client-server, and hybrid peer-to-peer modes.
- Animated robot and drone artwork, textured platforms, and a factory backdrop.
- Gravity, grounded jumps, collision response, and a rideable moving platform.
- **Client-owned robots; server-owned shared drone and moving platform.**
- Per-client pause and speed controls, with independent shared-world updates.
- HUD for controls, game status, player ID, connection, time scale, and render FPS.
- A 3200×1080 world with a following camera, two hidden spawn points, and collision-based death zones.
- Horizontal platform motion and a circular drone patrol, stored as scene components.
- Fourteen CTest targets: twelve non-socket tests pass at the revision recorded below; the two socket tests await authorization.

<a id="contents"></a>
## 📋 Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Technology Stack](#technology-stack)
- [Installation](#installation)
- [How to Play](#how-to-play)
- [Project Structure](#project-structure)
- [Milestone Progress](#milestone-progress)
- [Verification Guide and Limits](#verification-guide)

<a id="features"></a>
## ✨ Features

### Core Gameplay

- **Platforming:** A/D or arrow keys move the robot; Space jumps when grounded.
- **Physics:** Velocity-based movement, gravity, and separate visual/collision bounds.
- **Moving platform:** Standing robots receive the deck's observed displacement;
  walking or jumping off releases them. Carrying handles reversals and wall blocking.
- **Scaling:** P toggles constant-size and proportional rendering for the
  1920×1080 logical viewport over the larger world. The camera follows the robot; HUD text stays fixed.
- **Local completion:** Reaching the exit stops that robot and displays
  `ESCAPED! PRESS R TO PLAY AGAIN`. Other network players can continue.

### Multiplayer

- **Client-server:** `sessionClient` submits each client's robot state;
  `sessionServer` applies it and publishes world snapshots.
- **Hybrid peer-to-peer:** `peerSession` exchanges robot state directly with other
  peers. The coordinator still provides discovery, roster, drone, and platform state.
- **Replication:** `sceneReplicator` handles entity snapshots and stable network
  IDs. Versioned animation bytes travel as extra metadata; texture/sprite handles
  are reconstructed locally rather than sent over the network.
- **Local physics only:** Remote robots are visual replicas, not additional local
  rigid bodies or colliders. Remote players have gold backing markers.
- **Pause and speed:** T pauses one client's physics; 1/2/3 select 0.5×/1×/2×.
  Networking continues while paused, and a paused peer republishes its frozen
  robot so late joiners can receive it.
- **Lifecycle:** Distinct player IDs, late join, connection status, and removal of
  departed robots. Clients alternate between two designated spawns; spawn pairs can overlap until moved.

There is **no interpolation, prediction, or rollback** in the current game.
Remote motion follows received snapshots; network gaps can produce visible steps.
The server owns shared-world motion, not every player's movement simulation.

### Technical

SDL input/rendering runs on the main thread. The engine's `SimulationThread`
owns live gameplay state, and session workers own transport. Queued commands,
keyboard snapshots, and copied render frames separate those responsibilities.
The render loop targets 60 FPS; the FPS display measures presentation independently
of the paused/scaled game clock. This is a target, not a performance guarantee.

<a id="technology-stack"></a>
## 🛠️ Technology Stack

| Component | Current implementation |
| --- | --- |
| Language | C++20, required by the game and engine CMake targets |
| Graphics, input, images, fonts | SDL3, SDL3_image, SDL3_ttf through GEF_engine |
| Networking | ZeroMQ/cppzmq and the engine's session/replicator APIs |
| Supporting libraries | GLM, fmt, spdlog, nlohmann/json through the engine dependency configuration |
| Build | CMake 3.28 or newer; Ninja used for the recorded verification |
| Tests | CTest runs standalone C++ test executables; the game tests do not use Catch2 |

The verified configuration is **Linux, Debug**, against the exact engine revision
above. Other platforms and Release-build test behavior are not certified here.

<a id="installation"></a>
## 🔧 Installation

### Prerequisites

- Git, CMake 3.28+, Ninja, and a C++20-capable compiler/toolchain.
- A graphical environment supported by the configured SDL3 build for playing.
- The sibling GEF_engine checkout and its required dependency sources/libraries.

The engine's CMake dependency configuration supplies SDL3 and supporting libraries
and can use/build ZeroMQ as configured. Installing SDL2 packages is not the setup
for this code. A first online configuration can fetch dependencies; do that only
when downloads are intended. The recorded verification used cached sources with
fetching disabled.

### Setup Instructions

1. **Clone the game and engine side by side** and select the validated engine:

   ```sh
   mkdir robot-factory-workspace && cd robot-factory-workspace
   git clone https://github.com/ShounakDeshmukh/GEF_engine.git
   git -C GEF_engine checkout 0ec1cba862c91d2ed19bf2cf9b890cd51a5dede3
   git clone https://github.com/SuyeshJadhav/robot-factory-escape.git
   cd robot-factory-escape
   ```

2. **Configure and build with dependencies already cached:**

   ```sh
   cmake -S . -B build -G Ninja \
     -DCMAKE_BUILD_TYPE=Debug \
     -DGEF_ENGINE_DIR="$PWD/../GEF_engine" \
     -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
     -DENGINE_BUILD_TESTS=OFF \
     -DENGINE_BUILD_EXAMPLES=OFF \
     -DENGINE_BUILD_BENCH=OFF \
     -DGAME_BUILD_TESTS=ON &&
   cmake --build build --parallel 3
   ```

   `GEF_ENGINE_DIR` can point to another engine checkout. The game does not pin or
   update it automatically. If dependency sources are missing, the disconnected
   configure/build is not a first-time bootstrap: stop and intentionally arrange
   dependencies or configure with `FETCHCONTENT_FULLY_DISCONNECTED=OFF` only when
   downloads are acceptable. Do not silently change the engine revision to make
   a build pass.

3. **Run offline:**

   ```sh
   ./build/robot_factory_escape
   ```

   Runtime sprite/font paths are supplied by CMake from the game and engine
   checkouts; keep those assets available alongside the build.

### Note for graders: why a first build needs network access

This game does **not** download the engine. It compiles the engine from the local
source directory given by `-DGEF_ENGINE_DIR=`, so an engine checkout placed beside
the game (or shipped inside the submission archive) is sufficient.

The network requirement comes one level down, from the **engine's**
`cmake/Dependencies.cmake`, which resolves ten third-party libraries with CMake
`FetchContent` at *configure* time:

| Dependency | Pinned tag | Dependency | Pinned tag |
| --- | --- | --- | --- |
| SDL3 | `release-3.4.14` | nlohmann_json | `v3.12.0` |
| SDL3_image | `release-3.2.4` | fmt | `11.2.0` |
| SDL3_ttf | `release-3.2.2` | spdlog | `v1.17.0` |
| libzmq | `v4.3.5` | glm | `1.0.3` |
| cppzmq | `v4.11.0` | Catch2 (tests only) | `v3.15.3` |

Without network access, CMake aborts during configure, before any game source is
compiled. A configure failure of this kind is an environment limitation, not a
defect in the submitted game code.

**To build with no network at all,** pre-populate each dependency from a local
copy and configure fully disconnected:

```sh
cmake -S . -B build -G Ninja \
  -DGEF_ENGINE_DIR="$PWD/../GEF_engine" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
  -DFETCHCONTENT_SOURCE_DIR_SDL3=/path/to/SDL \
  -DFETCHCONTENT_SOURCE_DIR_SDL3_IMAGE=/path/to/SDL_image \
  -DFETCHCONTENT_SOURCE_DIR_SDL3_TTF=/path/to/SDL_ttf \
  -DFETCHCONTENT_SOURCE_DIR_LIBZMQ=/path/to/libzmq \
  -DFETCHCONTENT_SOURCE_DIR_CPPZMQ=/path/to/cppzmq \
  -DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON=/path/to/json \
  -DFETCHCONTENT_SOURCE_DIR_FMT=/path/to/fmt \
  -DFETCHCONTENT_SOURCE_DIR_SPDLOG=/path/to/spdlog \
  -DFETCHCONTENT_SOURCE_DIR_GLM=/path/to/glm \
  -DENGINE_BUILD_TESTS=OFF -DENGINE_BUILD_EXAMPLES=OFF -DENGINE_BUILD_BENCH=OFF
```

`FETCHCONTENT_SOURCE_DIR_<NAME>` uses the dependency name upper-cased. Reusing an
existing warm `_deps/` tree from a previous online build works the same way.

<a id="how-to-play"></a>
## ▶️ How to Play

### Controls

| Key | Action |
| --- | --- |
| A / Left Arrow | Move left |
| D / Right Arrow | Move right |
| Space | Jump; a new press while grounded is required |
| R | Restart the local game/robot, preserving the selected pause and speed |
| P | Toggle constant-size/proportional rendering |
| T | Pause/resume the local simulation |
| 1 / 2 / 3 | Select 0.5× / 1× / 2× local simulation speed |

### Game Objective

Reach the cyan-framed exit above the final platform at x=3000. Drone contact or
collision with a hidden pit/perimeter death zone resets the robot to its assigned
spawn. Jump across the floor gap or follow the raised platforms. Finishing and restarting are local to
each player; this is not a shared-winner or synchronized-round system.

Speed changes affect local physics and animation, **including jump timing**, not
only walking. In networked play, the server's platform and drone keep their own
clock. Restarting while paused leaves the robot suspended at spawn until T resumes;
that is the intended behavior, not an automatic unpause.

### Multiplayer Setup

> **Listener exposure:** the localhost addresses below do not confine every
> internal listener to loopback. Engine per-client handlers and peer publishers
> can bind `tcp://*:0`. Run only in an environment where those temporary listeners
> are permitted. Do not treat the launch flag below as isolation or a firewall.

Run each server/client command in a separate terminal from the game directory.
Stop one mode's processes before repeating the demo in the other mode. If a port
is occupied, inspect it rather than killing an unrelated process.

#### Client-Server Mode

```sh
# Terminal 1: server
./build/robot_factory_server --mode client-server \
  --bind tcp://127.0.0.1 --port 5555 --stats-interval-ms 3000

# Terminal 2: first client
./build/robot_factory_escape --mode client-server \
  --server-host 127.0.0.1 --server-port 5555 --initial-speed 1

# Terminal 3: second client
./build/robot_factory_escape --mode client-server \
  --server-host 127.0.0.1 --server-port 5555 --initial-speed 0.5
```

Add a third and fourth client by repeating a client command in separate terminals.
The original `network_sessions` test uses two clients; `factory_network_sessions`
adds four-client world, respawn, paused-late-join, and departure scenarios in both
modes. The latter is compiled but has not been run in the current implementation session.
Select `--initial-speed 2` for a 2× client; interactive 1/2/3 changes the same timeline.

#### Peer-to-Peer Mode

```sh
# Terminal 1: coordinator
./build/robot_factory_server --mode peer-to-peer \
  --bind tcp://127.0.0.1 --port 5556 --stats-interval-ms 3000

# Terminal 2: first peer
./build/robot_factory_escape --mode peer-to-peer \
  --server-host 127.0.0.1 --server-port 5556 --advertise-host 127.0.0.1

# Terminal 3: second peer; start after the first has joined
./build/robot_factory_escape --mode peer-to-peer \
  --server-host 127.0.0.1 --server-port 5556 --advertise-host 127.0.0.1 \
  --initial-speed 0.5
```

For a paused late-join check, pause the first peer with T before starting the
second. The second should receive the first robot's frozen state. Move robots
apart to inspect their individual artwork and animation.

**LAN is not verified by these localhost tests.** Off-host play requires a
reachable server bind/address; peer mode also needs each peer's reachable
`--advertise-host` and access to the engine-assigned ports. Merely replacing a
client's localhost address is insufficient. Network/firewall configuration is
separate from the verified local demo.

<a id="project-structure"></a>
## 📁 Project Structure

```text
robot-factory-escape/
├── assets/sprites/                       # Background, platforms, robot, drone
├── build/                                # Generated build; not source
├── include/robot_factory_escape/
│   ├── game.hpp
│   ├── game_settings.hpp
│   ├── gameplay/                         # Shared world, spawn/death rules, gameplay tick
│   ├── graphics/                         # Camera, sprite helpers and HUD labels
│   └── networking/                      # Fixed IDs, world binding, animation metadata
├── src/
│   ├── app/{main,game,game_loop}.cpp
│   ├── gameplay/                         # Factory world, respawn, tick, level, movement
│   ├── graphics/{game_render,game_sprites}.cpp
│   ├── networking/game_network.cpp
│   └── server/main.cpp
├── tests/
│   ├── CMakeLists.txt
│   └── *_tests.cpp                       # Fourteen registered test executables
├── CMakeLists.txt
└── README.md
```

<a id="milestone-progress"></a>
## 📊 Milestone Progress

### Milestone 1: Game and Engine Foundations

Implemented: game loop/rendering, entities, physics/gravity, input, collision
response, and constant/proportional scaling. The current game adds the factory
artwork, platform route, drone hazard, exit feedback, and HUD on those foundations.

### Milestone 2: Time and Networking Foundations

These capabilities predate the Milestone 3 compatibility regression:

| Area | Implementation and evidence |
| --- | --- |
| Local time manipulation (LO 5.1, 5.2) | Engine timelines support pause/resume and 0.5×/1×/2×; `network_state` checks controlled-clock behavior. |
| Headless server and simultaneous clients (LO 4.1, 4.2) | Separate server/client processes, distinct IDs and session workers; `network_sessions` exercises two clients in each mode. |
| Client-controlled replication (LO 4.3) | Each client owns its robot; IDs and animation metadata reconstruct remote visuals. |
| Concurrent processing (LO 6.3) | SDL input/rendering, simulation ownership, and transport workers are separated. |
| Asynchronous clients and shared world | Local simulation rates are independent; the server owns the moving platform and drone. |
| Hybrid peer-to-peer | Direct robot exchange, coordinator roster/world state, paused late join and departure cleanup. |

### Between Milestone 2 and the Current Validation

Compared with the Milestone 2 merge (`63d90bd`), subsequent build/gameplay work
already present at `ea13bf2`:

- Organized the former large game implementation into `app`, `gameplay`,
  `graphics`, and `networking` modules, plus separate test targets.
- Made the sibling engine checkout explicit through `GEF_ENGINE_DIR`.
- Refined platform carrying, deck collisions and detachment, with focused
  `moving_platform` and session regression checks.
- Moved restart handling to queued commands so R also works while paused.
- Refreshed shared-world state on the render loop's wall-clock cadence during
  pause rather than relying on sparse heartbeat updates.
- Added a 60 FPS render target and a presentation FPS readout independent of
  local simulation pause/speed.

These are part of the transition from the Milestone 2 snapshot, not new features
invented by the Milestone 3 test/documentation commits.

### Milestone 3: Individual Game Integration (current)

Implementation revision: `6d6bf6dfe8a12ee4e365cbebe81a714622533e79`. Engine revision remains
`0ec1cba862c91d2ed19bf2cf9b890cd51a5dede3`; no engine files changed.
The implementation uses ordinary structs and functions around the existing engine
Scene, physics, camera and networking APIs, not another ECS or transport layer.

| Part 2 requirement | Implementation | Evidence / remaining gate |
|---|---|---|
| Client-controlled movement and jumping | Existing input/movement code, shared `factory::tickPlayer` | Movement, carrying and gameplay tests pass; keyboard-driven demonstration pending. |
| Three distinct static platforms | Eight static platform specs with differing positions/widths in `factory_world.cpp` | `factory_world` passes. |
| Two movement patterns | Horizontal platform and circular drone components | `factory_motion` and production-tick carry/ownership tests pass. |
| Multiple hidden spawn points | `SpawnPoint`, explicit `RespawnTarget`, alternating client assignment | `factory_world` and `respawn` pass. |
| Hidden collision death zones | Pit and perimeter collider entities; real collision checks | `respawn` and `factory_gameplay` pass, including a playable pit crossing. |
| Scrolling camera | Bounded camera; world-space simulation; screen-anchored HUD | `camera` passes; actual rendering/scaling observation pending. |

**Verified:** Debug client/server/test builds, 12/12 non-socket tests, exact
14-target registration, and both binaries' `--help` paths. Sprite tests use SDL's
dummy driver, not a graphical game launch. **Not yet verified:** the new four-client
socket scenarios, the full suite, or the offline/four-window keyboard demonstrations.
A snapshot test does not measure keyboard-to-screen latency.

This does not complete the team engine submission, applicable CSC 581 experiments,
the individual paper/appendix, or submission packaging.

### Milestone 3: Updated Networking Refactor (historical scope)

**Assignment:** “Update your game to include the new networking refactor.”

**Status:** implemented and verified against that stated requirement with the
current engine revision. Complete graphical sign-off remains a separate limit.

| Change / check | What Milestone 3 establishes |
| --- | --- |
| Current engine integration | The production game/server call `sessionClient`, `sessionServer`, `peerSession` and `sceneReplicator` from engine revision `0ec1cba862c91d2ed19bf2cf9b890cd51a5dede3`. Existing session integration was retained; no second transport layer or ownership redesign was added. |
| New `network_refactor` regression | Real Binary/JSON round trips check robot ID/position, bound-entity reuse, exact animation metadata, local texture preservation, truncated-packet rejection and owner cleanup. Production sessions continue using default Binary. |
| Test registration | `tests/CMakeLists.txt` adds the compatibility target to the existing helper; eight tests are registered. |
| Repeatability and evidence | Disconnected Debug configure/build passed; each of the three focused networking tests passed five repetitions; the full suite passed 8/8 and was rerun before integration into main. |
| Independent review | Three read-only reviewers checked production integration, test/CMake validity and assignment/demo boundaries; no blocker to the stated refactor-adoption task was found. |

The milestone increment from `ea13bf2` added the compatibility test, its CMake
registration, and verification/change records. It did **not** add interpolation,
prediction, rollback, a shared winner, or synchronized restarts.

<a id="verification-guide"></a>
## 📖 Verification Guide and Limits

After the temporary-listener prerequisite above is satisfied:

```sh
export RFE_NETWORK_TESTS_AUTHORIZED=1

test "$RFE_NETWORK_TESTS_AUTHORIZED" = 1 &&
ctest --test-dir build -R '^network_(state|refactor|sessions)$' \
  --repeat until-fail:5 --no-tests=error --output-on-failure &&
ctest --test-dir build --no-tests=error --output-on-failure
```

The environment variable acknowledges permission to run socket checks; it does
not change the engine's bind addresses. Configure/build successfully first so
CTest is not exercising stale executables.

| Test | Scope |
| --- | --- |
| `player_motion` | Grounded jumps, motion, collision and input-related movement rules |
| `moving_platform` | Carrying, reversals, walking/jumping off, pause/resume, delayed updates and wall blocking |
| `game_progress` | Exit detection and one-shot win; restart is covered by `factory_gameplay` |
| `game_sprites` | Animation clips, metadata and local/remote sprite setup using headless SDL |
| `network_state` | IDs/metadata, remote physics exclusion and controlled-clock pause/speed |
| `network_refactor` | Current-engine Binary/JSON consumer compatibility and selected invalid-input/cleanup cases |
| `network_sessions` | Real client-server/peer sessions, robot/world delivery, platform carrying, paused reception, late join and departures |
| `factory_world` | Shared geometry, hidden components, distinct platforms and player creation |
| `factory_motion` | Horizontal/circular motion, resets and sustained bounds |
| `respawn` | Spawn assignment, collision death zones and reset state |
| `factory_gameplay` | Real tick, offline carry, client nonownership, restart/re-win and pit traversal |
| `camera` | Follow/clamps, fixed HUD, unchanged simulation/network state |
| `factory_replication` | Binary/JSON world bindings and cleanup with different local entity IDs; no sockets |
| `factory_network_sessions` | Four-client shared world, respawn, paused late join and disconnect in both modes; compiled, execution pending |

### What Is Not Claimed

- **Full graphical sign-off:** selected human pause/speed/late-join checks worked,
  but every manual control, riding, finish/replay and normal window-close check
  has not been individually certified. A gold backing rectangle is deliberately
  drawn for animated remote players; it alone is not a networking failure.
- **Release-test certification:** the existing `network_state` test has required
  operations inside `assert`; `NDEBUG` removes them. Recorded passing evidence
  is Debug-only. The new compatibility test uses runtime checks.
- **Exhaustive parser safety:** truncation checks cover one truncated encoded
  packet per representation and selected state, not arbitrary malformed packets
  or complete scene atomicity. JSON is tested at replicator level, not as a
  selectable graphical session mode.
- **Presentation polish after winning:** shared drone animation can freeze in
  the winner's window even while network updates move its position.
- **LAN, public-network, mixed-version or cross-architecture interoperability:**
  these are not established by localhost checks.

Current non-socket compatibility
checks pass; older session results do not certify the changed world or every
graphical/deployment scenario.

## Sources, assets, and submission attribution

| Material | Source / evidence | Submission status |
|---|---|---|
| Requirements | `CSC 481_581 Fall 2025 - Milestone 3` course brief | Cite the actual course instructions and confirm course-specific writeup/benchmark requirements. |
| Engine code/API | [Team GEF_engine](https://github.com/ShounakDeshmukh/GEF_engine), pinned revision above | Engine `LICENSE` is MIT, copyright 2026 The Deadlock Engine Authors; retain its notice when distributing covered code. |
| Background, platform, robot and drone sprites | Tracked PNGs under `assets/sprites/{background,platform,robot_sprite,drone}` | Original author/source URL and license evidence are not established here. Obtain them before submission; filenames alone are not permission. |
| HUD font | Engine's tracked `examples/hello_entity/assets/SuperBouncer.ttf` | Confirm font-specific source, license and required credit; do not infer a font license from the engine-code license. |
| Supporting libraries | SDL3/image/ttf, GLM, fmt, spdlog, nlohmann/json, cppzmq/ZeroMQ via engine dependencies | Retain applicable upstream notices if distributing them; no dependencies were downloaded or newly bundled in this change. |
| AI assistance | Hermes Agent (Nous Research) assisted with planning, implementation and tests for this increment | Disclose that assistance according to the course policy; recorded local build/test results are not a claim of unaided authorship. |

Missing sprite/font provenance is a submission-readiness blocker. No source archive,
paper, performance results, or actual course submission was produced in this run.
