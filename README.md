# Robot Factory Escape — learning sandbox

A runnable game consuming the team's `GEF_engine` repository. The verified
engine revision is [`155b85c`](https://github.com/ShounakDeshmukh/GEF_engine/commit/155b85c4fcea83bd25fe0bb5acc1b3d661953768).
Artwork is bundled in `assets/sprites/`.

For a fresh checkout, place the engine and game side by side:

```sh
mkdir milestone2 && cd milestone2
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

## Multiplayer networking and rubric demonstration

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
until a player moves. The HUD shows the mode, ID, connection status, pause, and speed.
The first platform travels between x=330 and x=450 at 60 logical pixels per
second. It has the same collider in every client, while its transform comes
from the server's timeline. A client pause stops its robot but does not stop
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

`T` pauses or resumes only the local simulation; `1`, `2`, and `3` set 0.5×,
1×, and 2× local speed. Other players, the server drone, and the moving platform continue while
one player is paused. A paused peer periodically republishes its frozen robot
so a late joiner can see it. `P` toggles rendering scale. Without network
options, the game runs offline. Winning and `R` restart are local to each
player.

| Rubric item | Implementation | Verification |
| --- | --- | --- |
| 4.1 Game server | `robot_factory_server` uses `sessionServer` and a server timeline | Launch commands above; session test |
| 4.2 Two clients | The server joins each client with a distinct ID and update worker | `network_sessions` test and two windows |
| 4.3 Entity replication | Client-owned robot `NetId`s and server-owned drone/platform `NetId`s use `sceneReplicator`; peer mode uses `peerSession` | `network_state` and `network_sessions` tests |
| 5.1/5.2 Timelines | Local game timeline supports pause, 0.5×, 1×, 2×; server has its own 60 Hz timeline | `network_state` clock test and HUD |
| 6.3 Concurrent processing | SDL render main thread, `SimulationThread` for gameplay, engine network workers for transport | Running two-client demo and session test |

The engine uses system ZeroMQ development packages when present, or builds
ZeroMQ from source. CMake may download dependencies on first configuration.

## What works

- Window, event loop, frame timing, and rectangle rendering.
- Cyberpunk backdrop and five textured raised platforms (the first moves) with an animated cyborg,
  drone, and code-drawn factory exit. The floor remains a basic shape.
- Matching shape/collider sizes. Only the robot has a rigid body.
- A/D moves the robot at 300 logical pixels per second. Releasing both keys stops
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
at any time: in offline mode the robot and drone patrol reset; in network mode
the robot resets while the server drone continues. The exit color resets. P still works after
winning, and restarting preserves the selected scaling mode.
Constant scaling can crop this room in a smaller window; proportional fits it.
The exit is elevated above a five-platform route, so reaching it requires a sequence
of jumps rather than simply walking across the factory floor.
Uphill steps rise 190 logical pixels, close to the roughly 211-pixel simulated jump
height. The middle platform provides a descent before the final two climbs; wait
for the drone to pass before crossing the exposed platforms.

## Where to work next

Game-specific gravity and jump speed live in `include/robot_factory_escape/game_settings.hpp`: gravity is
1800 logical pixels/s² and jump speed is 880 logical pixels/s. This gives roughly
a 215-pixel jump and one second of airtime when landing at the starting height
(slightly less height with discrete physics steps). `Game` passes gravity to the
engine's `PhysicsSystem` constructor; no shared engine modification is needed.
The engine also exposes `setGravity()` if the game needs to change it at runtime.

| Step | Location | Engine API |
| --- | --- | --- |
| 1. Room setup (done) | `createLevel()` | `createEntity`, `transform`, `addShape`, `addCollider` |
| 2. Movement and gravity (done) | `handleInput()`, `update()` | `isKeyPressed`, `getRigidBody`, `PhysicsSystem::step` |
| 3. Grounding and jumping (done) | `handleInput()`, `src/player_motion.cpp` | `isCollision`, `transform`, `getRigidBody` |
| 4. Patrol and respawn (done) | `update()`, `include/robot_factory_escape/drone_patrol.hpp` | `transform`, `isCollision`, `getRigidBody` |
| 5. Exit and restart (done) | `handleInput()`, `update()`, `include/robot_factory_escape/game_progress.hpp` | `isCollision` plus your own game state |
| 6. Scaling (done) | `handleInput()`, `render()` | `toggleScalingMode`, `drawEntities` |

`src/main.cpp` starts the game and reports errors. `Game` owns the engine objects and
scene. Components store data; calls inside the loop make that data do something.
Keep position changes for the robot in physics and collision response; update
the drone directly without a rigid body so gravity does not affect its patrol.

`src/player_motion.cpp` resolves horizontal and vertical movement separately using
the engine's overlap query. It advances in steps no larger than 1/120 second and
caps a frame's catch-up at 0.1 second. Long pauses therefore slow simulation rather
than advancing the entire pause at once. This is intended for this small level's
speeds and rectangular solids, not arbitrary high-speed collision detection.

Run the focused movement checks without opening a window:

```sh
cmake --build build --target player_motion_tests drone_patrol_tests game_progress_tests game_sprites_tests
ctest --test-dir build --output-on-failure
```

## Artwork

The supplied sprite folder now lives in `assets/sprites/`. The background is drawn
first across the 1920×1080 logical scene. Each platform uses the solid upper 1024×224
area of its source image and renders at 300×66. Its collision surface is a separate
300×24 rectangle across the top, so the player can travel through the transparent
space underneath or land on the deck. Source image artifacts are preserved.
Robot idle/walk/jump sheets and the drone movement sheet are animated by
`src/game_sprites.cpp`. Facing follows horizontal movement, jumping plays once, and
idle/walk/drone clips loop. Winning freezes playback; respawning resets it.
The sheets use inferred 512px robot cells and 256px drone cells. Their large
transparent margins are cropped separately for idle, walking, and jumping poses.
The robot renders at 96×128 with a tighter 80×128 body collider; the drone uses
130×80, so their visible scale better matches
the platform. Action/fire sheets
and the static drone image are retained but unused because there is no combat.
CMake supplies the asset directory, so running from a different working directory
is supported. Reconfigure CMake if you move this checkout.
