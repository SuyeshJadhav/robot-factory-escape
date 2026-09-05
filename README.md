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
- P toggles scaling once per press. Starts proportional at a 1920×1080 logical size.
- Closing the window exits cleanly.

This is a scaffold, not a playable game yet. Physics is deliberately paused until
collision response is implemented. A/D, Space, R, patrol, and winning remain TODOs.
Constant scaling can crop this room in a smaller window; proportional fits it.

## Where to work next

| Step | Location | Engine API |
| --- | --- | --- |
| 1. Room setup (done) | `createLevel()` | `createEntity`, `transform`, `addShape`, `addCollider` |
| 2. Movement and gravity | `handleInput()`, `update()` | `isKeyPressed`, `getRigidBody`, `PhysicsSystem::step` |
| 3. Grounding and jumping | `handleInput()`, `update()` | `isCollision`, `GetCollisionOverlap`, `transform` |
| 4. Patrol and respawn | `update()` | `transform`, `isCollision`, `getRigidBody` |
| 5. Exit and restart | `handleInput()`, `update()` | `isCollision`, `getShape` plus your own game state |
| 6. Scaling (done) | `handleInput()`, `render()` | `toggleScalingMode`, `drawEntities` |

`main.cpp` starts the game and reports errors. `Game` owns the engine objects and
scene. Components store data; calls inside the loop make that data do something.
Keep position changes for the robot in physics and collision response; update
the drone directly without a rigid body so gravity does not affect its patrol.
