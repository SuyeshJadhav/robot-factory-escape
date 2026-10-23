# Robot Factory Escape — Milestone 3

A platformer with offline play, client-server multiplayer, and hybrid peer-to-peer
multiplayer. Navigate the factory, avoid hazards, and reach the raised exit.

## Requirements

- Git, CMake 3.28+, Ninja, and a C++20 compiler (GCC or Clang).
- SDL3's system development libraries; see the
  [engine's platform setup instructions](https://github.com/ShounakDeshmukh/GEF_engine/blob/0ec1cba862c91d2ed19bf2cf9b890cd51a5dede3/README.md#linux).
- Internet access during the first configuration.
- A graphical desktop for game clients; the server runs without a window.

## Setup

Obtain **only the game** from the `milestone3` branch:

```sh
git clone --branch milestone3 --single-branch https://github.com/SuyeshJadhav/robot-factory-escape.git
cd robot-factory-escape
```

Alternatively, extract the Milestone 3 game submission archive or
[download the game ZIP](https://github.com/SuyeshJadhav/robot-factory-escape/archive/refs/heads/milestone3.zip)
and open a terminal in the extracted game directory. Git is still needed for
CMake's dependency downloads.

## Build

From the game directory:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
```

CMake automatically downloads engine **v0.3.0**, pinned to commit
`0ec1cba862c91d2ed19bf2cf9b890cd51a5dede3`, and its dependencies into
`build/_deps/`. No separate engine clone or sibling folder is needed; an existing
sibling engine is ignored by default. The first build may take several minutes.
Keep the source and build directories in place after building.

**Optional local engine:** add `-DGEF_ENGINE_DIR=/absolute/path/to/GEF_engine` to
use a complete v0.3.0 checkout, including its
`examples/hello_entity/assets/SuperBouncer.ttf` font. With an older CMake cache,
pass `-DGEF_ENGINE_DIR=` or use a fresh build directory to enable automatic download.

## Run

### Offline

```sh
./build/robot_factory_escape
```

### Client-server multiplayer

Open separate terminals in the game directory. Start the server in **terminal 1**:

```sh
./build/robot_factory_server --mode client-server --bind tcp://127.0.0.1 --port 5555
```

Run this command in **each additional terminal** for up to four players:

```sh
./build/robot_factory_escape --mode client-server --server-host 127.0.0.1 --server-port 5555
```

### Hybrid peer-to-peer multiplayer

Close the previous clients and stop the previous server before switching modes.
Start the coordinator in **terminal 1**:

```sh
./build/robot_factory_server --mode peer-to-peer --bind tcp://127.0.0.1 --port 5556
```

Run this command in **each additional terminal** for up to four players:

```sh
./build/robot_factory_escape --mode peer-to-peer --server-host 127.0.0.1 --server-port 5556 --advertise-host 127.0.0.1
```

Use the same mode for the server and all clients. These examples run on one
computer, not a LAN. Close game windows to quit; press Ctrl+C to stop the server.

## Controls

| Key | Action |
| --- | --- |
| A / D or Left / Right | Move left / right |
| Space | Jump while grounded |
| T | Pause / resume your local simulation |
| 1 / 2 / 3 | Half / normal / double local speed |
| R | Restart your robot |
| P | Toggle display scaling |
| Close window | Quit |

Pausing one client does not pause the server's shared world or other clients.

## Credits

- Requirements: CSC 481/581 Fall 2025, Milestone 3 course brief.
- Engine: [GEF_engine](https://github.com/ShounakDeshmukh/GEF_engine), by The Deadlock
  Engine Authors, under its [MIT license](https://github.com/ShounakDeshmukh/GEF_engine/blob/0ec1cba862c91d2ed19bf2cf9b890cd51a5dede3/LICENSE).
- Sprites: course-provided artwork from the [NC State DELTA CSC 481 sprite
  selector](https://issdev.delta.ncsu.edu/courses/csc481-sprite-selector/), stored in
  `assets/sprites/`. No redistribution permission beyond coursework is claimed.
- HUD font: [Super Bouncer by fsuarez913](https://www.dafont.com/super-bouncer.font),
  supplied through the engine's example assets; its terms are separate from the
  engine's code license.
- Supporting libraries: SDL3/image/ttf, GLM, fmt, spdlog, nlohmann/json, and
  cppzmq/ZeroMQ, fetched by the engine under their respective upstream licenses.
- AI assistance: Hermes Agent (Nous Research) assisted with planning,
  implementation, debugging, and verification.
