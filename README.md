# Robot Factory Escape — learning sandbox

A runnable scaffold consuming the engine in the repository two directories above.
Artwork is bundled in `assets/sprites/`; no additional game dependencies are needed. Open this directory as a
CMake project in CLion, or use the commands below from this directory.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target robot_factory_escape
./build/robot_factory_escape
```

The engine's dependency setup may download dependencies during configuration.
Build output stays in this directory's ignored `build/` folder.

## What works

- Window, event loop, frame timing, and rectangle rendering.
- Cyberpunk backdrop and a textured raised platform with an animated cyborg,
  drone, and code-drawn factory exit. The floor remains a basic shape.
- Matching shape/collider sizes. Only the robot has a rigid body.
- A/D moves the robot at 300 logical pixels per second. Releasing both keys stops
  horizontal movement; holding both cancels out. Gravity accelerates it downward.
- The robot lands on the floor and platform, stops against the platform's sides, and
  stops rising when it hits an underside. Space jumps once per press while grounded.
- The robot starts above the floor and resets its position and velocity after
  falling below the screen (for example, after walking off the floor's end).
- P toggles scaling once per press. Starts proportional at a 1920×1080 logical size.
- Closing the window exits cleanly.

Movement, solid collisions, jumping, drone patrol, winning, and restart are implemented. The drone
moves between x=500 and x=1600 at 180 logical pixels per second without gravity.
Contact resets the robot position, velocity, and grounded state; the drone keeps
its patrol progress. The exit uses a cyan frame with orange factory accents;
touching it turns its status lights green and freezes gameplay. Press R to restart
at any time: the robot, drone patrol, and exit color reset. P still works after
winning, and restarting preserves the selected scaling mode.
Constant scaling can crop this room in a smaller window; proportional fits it.

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
first across the 1920×1080 logical scene. The platform uses the solid upper 1024×224
area of its source image and renders at 360×79. Its collision surface is a separate
360×28 rectangle across the top, so the player can travel through the transparent
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
