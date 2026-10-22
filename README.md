# Robot Factory Escape — Milestone 2

A platformer with offline play, client-server multiplayer, and hybrid peer-to-peer
multiplayer. Climb the platforms, avoid the drone, and reach the cyan exit.

## Requirements

- Git, CMake 3.28+, Ninja, and a C++20 compiler (GCC or Clang).
- SDL3's system development libraries; see the
  [engine's platform setup instructions](https://github.com/ShounakDeshmukh/GEF_engine/blob/155b85c4fcea83bd25fe0bb5acc1b3d661953768/README.md#required-toolchain).
- Internet access during the first configuration.
- A graphical desktop for game clients; the server runs without a window.

## Setup

Obtain **only the game**. Once the corrected `milestone2` branch is published:

```sh
git clone --branch milestone2 --single-branch https://github.com/SuyeshJadhav/robot-factory-escape.git
cd robot-factory-escape
```

Alternatively, extract the Milestone 2 game submission archive or
[download the game ZIP](https://github.com/SuyeshJadhav/robot-factory-escape/archive/refs/heads/milestone2.zip)
and open a terminal in the extracted game directory. Git is still needed for
CMake's dependency downloads.

## Build

From the game directory:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
```

CMake automatically downloads engine **v0.2.0**, pinned to commit
`155b85c4fcea83bd25fe0bb5acc1b3d661953768`, and its dependencies, including
ZeroMQ, into `build/_deps/`. No separate engine clone or sibling folder is needed;
an existing sibling engine is ignored by default. The first build may take
several minutes. Keep the source and build directories in place after building.

**Optional local engine:** add `-DGEF_ENGINE_DIR=/absolute/path/to/GEF_engine` to
the configure command to use a complete v0.2.0 checkout instead. Include its
`examples/hello_entity/assets/SuperBouncer.ttf` font. With an older CMake cache,
pass `-DGEF_ENGINE_DIR=` or use a fresh build directory to enable automatic download.

## Run

### Offline

```sh
./build/robot_factory_escape
```

### Client-server multiplayer

Use four terminals on the same computer, all opened in the game directory.
Start the server first in **terminal 1**:

```sh
./build/robot_factory_server --mode client-server --bind tcp://127.0.0.1 --port 5555
```

Then run this command once in **each of terminals 2, 3, and 4**:

```sh
./build/robot_factory_escape --mode client-server --server-host 127.0.0.1 --server-port 5555
```

Each client controls a separate robot. All robots start at the same position;
move one to see the others. The server controls the shared drone and moving
platform. Start the third client later to try joining an existing session.

### Hybrid peer-to-peer multiplayer

Close the previous clients and stop the previous server before switching modes.
Start the coordinator in **terminal 1**:

```sh
./build/robot_factory_server --mode peer-to-peer --bind tcp://127.0.0.1 --port 5556
```

Run this command once in **each of terminals 2, 3, and 4**:

```sh
./build/robot_factory_escape --mode peer-to-peer --server-host 127.0.0.1 --server-port 5556 --advertise-host 127.0.0.1
```

Peers exchange robot state directly; the coordinator supplies discovery and the
shared world. Use the same mode for the server and all its clients. These examples
are for one computer, not a LAN setup. Close game windows to quit clients;
press Ctrl+C in the server terminal to stop the server.

## Controls

| Key | Action |
| --- | --- |
| A / D or Left / Right | Move left / right |
| Space | Jump while grounded |
| T | Pause / resume your local simulation |
| 1 / 2 / 3 | Half / normal / double local speed |
| R | Restart |
| P | Toggle display scaling |
| Close window | Quit |

Pausing one client does not pause the server's moving platform or the other clients.
