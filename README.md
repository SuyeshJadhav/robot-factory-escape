# Robot Factory Escape

A cyberpunk-themed platformer built with the team's GEF engine. Escape the robot
factory by navigating platforms, avoiding the patrol drone, and reaching the
raised exit. Play offline or connect multiple game processes using client-server
or hybrid peer-to-peer networking.

## Quick Start for Graders

**This individual submission contains the game. Use the engine from our separate
team submission.** The game was verified against GEF_engine 0.3.0, revision
`0ec1cba862c91d2ed19bf2cf9b890cd51a5dede3`.

### 1. Place the two submissions beside each other

Extract the engine and game into this layout, with each folder's `CMakeLists.txt`
directly inside it:

```text
grading-workspace/
├── GEF_engine/              # Separately submitted team engine
│   ├── CMakeLists.txt
│   └── ...
└── robot-factory-escape/    # This individual game submission
    ├── CMakeLists.txt
    ├── assets/
    ├── include/
    └── src/
```

Keep the complete engine folder, including
`examples/hello_entity/assets/SuperBouncer.ttf`, which the game uses for its HUD.
The game's CMake configuration includes the engine automatically; no edit to the
engine's `CMakeLists.txt` is needed.

### 2. Build from the game folder

Requires Git, CMake 3.28+, Ninja, a C++20 compiler, and a graphical desktop.
Install the operating-system dependencies listed in `GEF_engine/README.md` first.
From `grading-workspace/`, run:

```sh
cd robot-factory-escape

cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DGEF_ENGINE_DIR="$PWD/../GEF_engine" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=OFF \
  -DENGINE_BUILD_EXAMPLES=OFF \
  -DENGINE_BUILD_BENCH=OFF \
  -DGAME_BUILD_TESTS=OFF

cmake --build build --parallel 3
```

**The first build needs internet access** to download third-party dependencies.
If the engine is in another location, replace the `GEF_ENGINE_DIR` value with its
absolute path. Build this source copy on the grader's machine; keep the game
assets and engine font at their configured paths after building.

### 3. Run the game

From the same game folder, start offline mode with:

```sh
./build/robot_factory_escape
```

For multiplayer on one computer, run each command below in a **separate terminal
from the game folder**, starting the server first. Repeat a client command in
additional terminals for up to four players. Stop the clients and server before
switching modes; Ctrl+C stops a headless server.

Client-server mode:

```sh
# Terminal 1: server
./build/robot_factory_server --mode client-server --bind tcp://127.0.0.1 --port 5555

# Terminal 2 onward: one client per terminal
./build/robot_factory_escape --mode client-server --server-host 127.0.0.1 --server-port 5555
```

Hybrid peer-to-peer mode:

```sh
# Terminal 1: coordinator and shared-world server
./build/robot_factory_server --mode peer-to-peer --bind tcp://127.0.0.1 --port 5556

# Terminal 2 onward: one peer per terminal
./build/robot_factory_escape --mode peer-to-peer --server-host 127.0.0.1 --server-port 5556 --advertise-host 127.0.0.1
```

## Controls

| Key | Action |
| --- | --- |
| A / Left Arrow | Move left |
| D / Right Arrow | Move right |
| Space | Jump while grounded |
| T | Pause or resume |
| P | Toggle display scaling |
| R | Restart your robot |
| 1 / 2 / 3 | Half / normal / double speed |
| Close window | Quit |
