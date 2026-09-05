# Robot Factory Escape — learning sandbox

A runnable scaffold consuming the engine in the repository two directories above.
No textures or additional game dependencies are needed. Open this directory as a
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
- Cyan robot, gray floor, orange crate, red drone, and green exit.
- Matching shape/collider sizes. Only the robot has a rigid body.
- A/D moves the robot at 300 logical pixels per second. Releasing both keys stops
  horizontal movement; holding both cancels out. Gravity accelerates it downward.
- The robot lands on the floor and crate, stops against the crate's sides, and
  stops rising when it hits an underside. Space jumps once per press while grounded.
- The robot starts above the floor and resets its position and velocity after
  falling below the screen (for example, after walking off the floor's end).
- P toggles scaling once per press. Starts proportional at a 1920×1080 logical size.
- Closing the window exits cleanly.

Movement, solid collisions, jumping, and drone patrol are implemented. The drone
moves between x=950 and x=1500 at 180 logical pixels per second without gravity.
Contact resets the robot position, velocity, and grounded state; the drone keeps
its patrol progress. R and winning remain TODOs; the exit is currently decorative.
Constant scaling can crop this room in a smaller window; proportional fits it.

## Where to work next

| Step | Location | Engine API |
| --- | --- | --- |
| 1. Room setup (done) | `createLevel()` | `createEntity`, `transform`, `addShape`, `addCollider` |
| 2. Movement and gravity (done) | `handleInput()`, `update()` | `isKeyPressed`, `getRigidBody`, `PhysicsSystem::step` |
| 3. Grounding and jumping (done) | `handleInput()`, `player_motion.cpp` | `isCollision`, `transform`, `getRigidBody` |
| 4. Patrol and respawn (done) | `update()`, `drone_patrol.hpp` | `transform`, `isCollision`, `getRigidBody` |
| 5. Exit and restart | `handleInput()`, `update()` | `isCollision`, `getShape` plus your own game state |
| 6. Scaling (done) | `handleInput()`, `render()` | `toggleScalingMode`, `drawEntities` |

`main.cpp` starts the game and reports errors. `Game` owns the engine objects and
scene. Components store data; calls inside the loop make that data do something.
Keep position changes for the robot in physics and collision response; update
the drone directly without a rigid body so gravity does not affect its patrol.

`player_motion.cpp` resolves horizontal and vertical movement separately using
the engine's overlap query. It advances in steps no larger than 1/120 second and
caps a frame's catch-up at 0.1 second. Long pauses therefore slow simulation rather
than advancing the entire pause at once. This is intended for this small level's
speeds and rectangular solids, not arbitrary high-speed collision detection.

Run the focused movement checks without opening a window:

```sh
cmake --build build --target player_motion_tests drone_patrol_tests
ctest --test-dir build --output-on-failure
```
