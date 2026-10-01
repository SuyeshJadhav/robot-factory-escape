# Robot Factory Escape

A runnable game consuming the team's `GEF_engine` repository. The verified
engine revision is [`155b85c`](https://github.com/ShounakDeshmukh/GEF_engine/commit/155b85c4fcea83bd25fe0bb5acc1b3d661953768).
Artwork is bundled in `assets/sprites/`.

For a fresh checkout, place the engine and game side by side:

```sh
mkdir robot-factory-workspace && cd robot-factory-workspace
git clone https://github.com/ShounakDeshmukh/GEF_engine.git
git -C GEF_engine checkout 155b85c
git clone https://github.com/SuyeshJadhav/robot-factory-escape.git
cd robot-factory-escape
```

If the engine is already elsewhere, pass
`-DGEF_ENGINE_DIR=/absolute/path/to/GEF_engine` to the CMake configure
command. Then build and run the game:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target robot_factory_escape
./build/robot_factory_escape
```

## Milestone task status

Status checked against the current implementation on **2026-10-01**. The seven
game test executables pass. The tables distinguish implemented features from
remaining verification and submission work.

### Milestone 1: Game and engine foundations

These six task groups follow the Milestone 1 reflection. They describe the
foundation retained in the current game; Milestone 2 extends it with shared
moving objects, timelines, networking, and concurrency.

| Task | Status | Implementation in this game | Evidence / how to verify |
| --- | --- | --- | --- |
| 1. Game loop and rendering | Implemented | SDL window and renderer, background, platform route, animated robot/drone, exit, and HUD. The current loop separates rendering from fixed-step simulation. | Launch offline; verify the scene renders, resizing works, and closing exits cleanly. See `src/app/game_loop.cpp` and `src/graphics/game_render.cpp`. |
| 2. Entity system | Implemented | Robot, floor, platforms, drone, exit, and HUD use engine entities with only the components they need. Visual sizes and colliders are separate. | See `src/gameplay/level.cpp` and `include/robot_factory_escape/gameplay/level_layout.hpp`; inspect the five-platform level. |
| 3. Physics and gravity | Implemented; regression tested | Velocity-based movement, configurable gravity, grounded jumps, and fixed-size collision substeps. Tuning remains in the game. | `player_motion` tests falling, landing, jump height/airtime, and slow frames. Jump and release movement keys in the game. |
| 4. Input handling | Implemented | A/D or arrows move; Space jumps once per press; R restarts; P toggles scaling. Milestone 2 adds T and 1/2/3. | Hold/release movement keys; hold Space to confirm it does not repeatedly jump. Restart and scaling use press edges. |
| 5. Collision detection and response | Implemented; regression tested | Solid tops, sides, and undersides; stable grounding; drone-contact respawn; fall reset; exit trigger and win/restart cycle. | `player_motion`, `drone_patrol`, and `game_progress`; play through the platform route and reach the elevated exit. |
| 6. Constant and proportional scaling | Implemented | P switches between constant pixel size and fitting the 1920x1080 logical scene while preserving aspect ratio. Physics coordinates remain unchanged. | Resize the window and press P. Proportional mode fits the room; constant mode can crop it in a small window. |

Additional game presentation: cropped animation sheets, readable HUD, common
spawn, and an elevated exit give the engine features a complete gameplay loop.
`game_sprites` checks animation setup and transitions.

### Milestone 2: Time and networking foundations

Section numbers below follow the Project 2 brief. The supplied weekly rubric
maps server/client/replication work to **LO 4.1-4.3**, timelines to **LO 5.1-5.2**,
and concurrent processing to **LO 6.3** (CC 8).

| Assignment task | Status | Implementation in this game / engine | Evidence / how to verify |
| --- | --- | --- | --- |
| Section 1: Represent and manipulate time (LO 5.1, 5.2) | Implemented; regression tested | Engine `Timeline` supports a parent clock, adjustable tick rate, pause/resume, and speed scaling. The game uses separate 60 Hz local and server timelines; T pauses locally and 1/2/3 select 0.5x/1x/2x. | `network_state` checks local pause/speed. Compare two clients' time HUDs and motion. |
| Section 2: Headless game server (LO 4.1) | Implemented | `robot_factory_server` uses the engine's ZeroMQ `sessionServer` and advances server-owned world objects without an SDL window. | Start the server using the commands below; `network_sessions` exercises real localhost sessions. |
| Section 2: Simultaneous client processes (LO 4.2) | Implemented; two automated / three demonstrated | Distinct client IDs and per-client request/reply workers. The weekly task asks for two clients; the full Project 2 brief requires at least three separate processes and late joins. | `network_sessions` starts two clients in each mode. A three-client client-server run was captured for the reflection; use the three-client commands below to repeat it. |
| Section 2: Client-controlled entity replication (LO 4.3) | Implemented; regression tested | Each client owns one robot. Client-server mode relays robot snapshots; stable network IDs and animation metadata reconstruct remote visuals. All robots share the same spawn. | `network_state` and `network_sessions`; move one robot and observe it in the other windows, then close a client to verify removal. |
| Section 3: Multithreaded loop architecture (LO 6.3) | Implemented | Main thread handles SDL input/rendering, `SimulationThread` owns live game state, and engine network workers own transport. Copied frames, latest-value handoffs, and queued commands separate access. | Run multiple clients; see `src/app/game_loop.cpp` and the engine session APIs. The client processes have separate scenes, with no shared gameplay memory. |
| Section 4: Asynchronous clients at different speeds | Implemented; measured locally | Synchronous ZeroMQ request/reply communication runs through a dedicated server worker for each client, without ROUTER/DEALER. Local time scaling changes simulation/state publication rate independently. | Run 0.5x and 2x clients: while unpaused, server request counters should rise near 30 and 120 per second. The shared world remains at the server's 60 Hz rate. |
| Section 4: Server-governed moving platforms | Implemented; riding fix verified | The server owns the first platform and drone in both modes. A standing robot receives the platform's observed displacement; jumping or walking off releases it. | `moving_platform` and `network_sessions`; ride through a reversal in both windows. Pause one robot and confirm the world keeps moving. |
| Section 5: Hybrid peer-to-peer | Implemented; regression tested | Coordinator supplies roster/shared world; peers send robot state directly. Paused peers republish frozen state so late joiners can receive it. | Start peer mode below. `network_sessions` checks direct replication, frozen-state late joins, shared platforms, and departures. |
| Optional fully decentralized peer negotiation | Not implemented | This game uses the permitted hybrid model and retains a coordinator for world authority and discovery. | No claim is made for the extra-credit option that removes centralized authority. |

### Latest fixes and verification limits

| Item | Status |
| --- | --- |
| Robot riding the moving platform | Fixed and tested offline, client-server, and peer-to-peer; includes reversals, walking/jumping off, delayed updates, and wall blocking. |
| Restart while paused | Fixed; scripted gameplay checks verified return to the common spawn while keeping pause selected. |
| Sparse shared-world updates during pause | Fixed in both modes. Native-window checks measured roughly 58-59 world updates/sec during pause, previously about 10 in client-server and 4 in peer mode. |
| Render pacing and FPS visibility | Implemented: 60 FPS target with an independent FPS readout. Local two-client checks stayed near 60 FPS; actual performance depends on machine load. |
| Reported FPS drop in other, unpaused windows | Not reproduced in the native-window checks; an actual cross-window rendering slowdown is not conclusively diagnosed. Use the FPS readout to distinguish it from sparse network updates. |
| Two-machine LAN operation | Not yet verified. All recorded network checks used localhost; the launch options accept reachable LAN addresses. |
| Interpolation / prediction | Not implemented. Remote movement uses received snapshots, so network delay or packet gaps can still cause visible stepping. |
| Reflections and submission packaging | Milestone 1 and 2 reflections were prepared separately from this repository. Review the Milestone 2 text/screenshots against the latest fixes before submission; upload/submission status is not tracked here. |

## Milestone 2: Run and demonstrate multiplayer

Build the game, headless server, and tests from this directory:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Run one server and three clients in four terminals. Client-server is the default
network mode; these commands also work across machines when `--server-host` is
set to the server's reachable address.

```sh
./build/robot_factory_server --mode client-server --bind 'tcp://*' --port 5555 --stats-interval-ms 3000
./build/robot_factory_escape --mode client-server --server-host 127.0.0.1 --server-port 5555
./build/robot_factory_escape --mode client-server --server-host 127.0.0.1 --server-port 5555
./build/robot_factory_escape --mode client-server --server-host 127.0.0.1 --server-port 5555
```

The server assigns distinct player IDs, relays each client's robot through its
scene snapshot, and advances the shared drone and first platform at 60 Hz. Each window shows its
own animated robot, other players' robots with gold outlines, the server's drone,
and the cyan-outlined moving platform. Closing a client removes its robot from the other
window. All robots spawn and restart at the same position, so they may overlap
until a player moves. The HUD shows the mode, ID, connection status, pause, speed, and measured render FPS.
See [the verification guide](docs/multiplayer-verification.md) for the full manual checklist.

The first platform travels between x=330 and x=450 at 60 logical pixels per
second. It has the same collider in every client, while its transform comes
from the server's timeline. Standing robots ride with the platform, including at
patrol reversals; walking and jumping remain relative to the robot's own controls.
Platform displacement is applied once per received update, independently of
local speed. A client pause stops its robot but does not stop
the shared platform or drone.
Start the third client after the first two are running to check late joins.
The optional server statistics print cumulative per-client request, state, and
snapshot counts every three seconds.

To compare asynchronous client rates without coordinating key presses, start
one client with `--initial-speed 0.5` and another with `--initial-speed 2`.
Subtract two consecutive `updates` counts in the server log: steady-state
rates should be about 30 and 120 requests per second, respectively. The
interactive `1`, `2`, and `3` keys change the same local timeline afterward.

The alternative peer-to-peer mode keeps the server as a roster and shared-world
coordinator while clients exchange their robots directly:

```sh
./build/robot_factory_server --mode peer-to-peer --bind 'tcp://*' --port 5555 --stats-interval-ms 3000
./build/robot_factory_escape --mode peer-to-peer --server-host 127.0.0.1 --server-port 5555 --advertise-host 127.0.0.1
./build/robot_factory_escape --mode peer-to-peer --server-host 127.0.0.1 --server-port 5555 --advertise-host 127.0.0.1
./build/robot_factory_escape --mode peer-to-peer --server-host 127.0.0.1 --server-port 5555 --advertise-host 127.0.0.1
```

For a LAN peer session, `--advertise-host` must be that client's reachable IP.
Allow the server join port and engine-assigned per-client control ports through
firewalls; peer mode also needs access to engine-assigned direct peer ports.

`R` also works while paused and preserves the selected pause/speed.
`T` pauses or resumes only the local simulation; `1`, `2`, and `3` set 0.5×,
1×, and 2× local speed. Other players, the server drone, and the moving platform continue while
one player is paused. A paused peer periodically republishes its frozen robot
so a late joiner can see it. Paused clients refresh shared-world state on the
render loop's wall-clock cadence, rather than waiting for a slow connection
heartbeat. The render loop targets 60 FPS to leave resources for other client
windows; this does not cap the 2x simulation's 120 ticks per second.
`P` toggles rendering scale. Without network
options, the game runs offline. Winning and `R` restart are local to each
player.

The engine uses system ZeroMQ development packages when present, or builds
ZeroMQ from source. CMake may download dependencies on first configuration.

## What works

- Window, event loop, frame timing, and rectangle rendering.
- Cyberpunk backdrop and five textured raised platforms (the first moves) with an animated cyborg,
  drone, and code-drawn factory exit. The floor remains a basic shape.
- Separate visual and collision bounds matched to the solid artwork. Only the local robot has a rigid body during gameplay.
- A/D or the left/right arrow keys move the robot at 300 logical pixels per second. Releasing both keys stops
  horizontal movement; holding both cancels out. Gravity accelerates it downward.
- The robot lands on the floor and platforms, stops against platform sides, and
  stops rising when it hits an underside. Space jumps once per press while grounded.
- The robot starts above the floor and resets its position and velocity after
  falling below the screen (for example, after walking off the floor's end).
- P toggles scaling once per press. Starts proportional at a 1920×1080 logical size.
- The engine text module renders a HUD with controls, the objective, and live status.
- Closing the window exits cleanly.

Movement, solid collisions, jumping, drone patrol, winning, and restart are implemented. The drone
moves between x=500 and x=1600 at 180 logical pixels per second without gravity.
Its patrol is at y=440, crossing the robot's path on the second and fourth platforms.
Contact resets the robot position, velocity, and grounded state; the drone keeps
its patrol progress. The exit uses a cyan frame with orange factory accents;
touching it turns its status lights green and freezes local gameplay. Press R to restart
at any time: in offline mode the robot, drone, and moving platform reset; in network mode
the robot resets while the server drone continues. The exit color resets. P still works after
winning, and restarting preserves the selected scaling mode.
Constant scaling can crop this room in a smaller window; proportional fits it.
The exit is elevated above a five-platform route, so reaching it requires a sequence
of jumps rather than simply walking across the factory floor.
Uphill steps rise 190 logical pixels, close to the roughly 211-pixel simulated jump
height. The middle platform provides a descent before the final two climbs; wait
for the drone to pass before crossing the exposed platforms.

## Project structure

```text
src/
  app/          Entry point, Game construction, window/simulation loop
  gameplay/     Level construction, input, progress, movement and collisions
  graphics/     Rendering, exit artwork, sprite animation
  networking/   Session setup, incoming state, outgoing robot snapshots
  server/       Headless authoritative world and server CLI
include/robot_factory_escape/
  game.hpp                Game orchestration and state ownership
  game_settings.hpp       Movement tuning
  gameplay/               Rules, patrols, and shared level geometry
  graphics/               Sprite and HUD helpers
  networking/             Replication metadata
tests/                     Regression tests and their CMake configuration
docs/                      Manual verification guide
```

`gameplay/level_layout.hpp` is the shared source of platform, robot, and drone
geometry. Both server and client use it so replicated collision bounds match.
`game_settings.hpp` contains gravity (1800), jump speed (880), and run speed (300),
all in logical pixels and seconds. The engine checkout stays independent of the
game's rules.

`gameplay/player_motion.cpp` resolves axes separately with physics steps no larger
than 1/120 second and a catch-up cap of 0.1 second. It is tuned for this level's
rectangular solids, rather than arbitrary high-speed collision detection.
Platform riding uses observed displacement only while the robot touches the
old deck. Carry is swept against other solids and does not alter input velocity.
Offline updates and network snapshots use the same carry routine. Paused clients
discard carry deltas, so resuming never applies a backlog of platform motion.

Game state changes belong to the simulation thread. The main thread posts
restart/time controls (including during pause) and renders copied scenes.
Network updates are applied on the simulation thread even between local ticks.

Build and run all checks with:

```sh
cmake --build build
ctest --test-dir build --output-on-failure
```

The repository's `.clang-format` keeps C++ formatting consistent.

## Artwork

The supplied sprite folder now lives in `assets/sprites/`. The background is drawn
first across the 1920×1080 logical scene. Each platform uses the solid upper 1024×224
area of its source image and renders at 300×66. Its collision surface is a separate
300×24 rectangle across the top, so the player can travel through the transparent
space underneath or land on the deck. Source image artifacts are preserved.
Robot idle/walk/jump sheets and the drone movement sheet are animated by
`src/graphics/game_sprites.cpp`. Facing follows horizontal movement, jumping plays once, and
idle/walk/drone clips loop. Winning freezes playback; respawning resets it.
The sheets use inferred 512px robot cells and 256px drone cells. Their large
transparent margins are cropped separately for idle, walking, and jumping poses.
The robot renders at 96×128 with a tighter 80×128 body collider; the drone uses
130×80, so their visible scale better matches
the platform. Action/fire sheets
and the static drone image are retained but unused because there is no combat.
CMake supplies the asset directory, so running from a different working directory
is supported. Reconfigure CMake if you move this checkout.
