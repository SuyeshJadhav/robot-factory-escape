# Robot Factory Escape — Milestone 4

A platformer built on the team's GEF engine. Navigate the factory, avoid the red
patrol drone and death zones, and reach the cyan exit. Supports offline play,
client-server multiplayer, hybrid peer-to-peer multiplayer, and manual replay.
Input, lethal collisions, death, and spawn are handled through engine events.
Remote death/spawn HUD notices are delivered through the network event channel.

## Quick start for graders

**Build the game from this repository. No separate engine clone, sibling folder,
or edit to the engine is required.** CMake downloads the released engine
**v0.4.0**, pinned to immutable commit
`603154b11de36bbf837605fa3da43d949b3f4a5f`, and its third-party dependencies.

The commands below use a **Linux/macOS shell and Ninja**, not the engine's presets.
Linux is the validated platform; macOS and Windows builds have not been verified
for this game. Do not copy a Linux executable to Windows or macOS and expect it to
run. Build the source on the destination machine.

### 1. Check prerequisites

- Git, Ninja, and a C++20 compiler (GCC or Clang).
- CMake: the project declares **3.28+**; the documented build is verified with
  **CMake 4.3.0, Ninja 1.13.2, and GCC 16.2.1** on Fedora Linux. The engine's own
  preset instructions list different tool versions; these commands do not use
  those presets or require their `ccache` launcher.
- Linux development libraries for an SDL desktop backend. For an X11/Xwayland
  build, example prerequisite commands are:

  **Ubuntu/Debian:**
  ```sh
  sudo apt-get update
  sudo apt-get install build-essential git cmake ninja-build pkg-config libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev libxkbcommon-dev
  ```

  **Fedora:**
  ```sh
  sudo dnf install gcc-c++ git cmake ninja-build pkgconf-pkg-config libX11-devel libXext-devel libXrandr-devel libXcursor-devel libXi-devel libXfixes-devel libXScrnSaver-devel libxkbcommon-devel
  ```

  These are X11-focused examples, not a full native-Wayland/audio/GPU dependency
  list or a clean-OS installation test. See the
  [pinned engine's Linux setup](https://github.com/ShounakDeshmukh/GEF_engine/blob/603154b11de36bbf837605fa3da43d949b3f4a5f/README.md#linux)
  for its broader dependency list and its macOS section for platform setup.
- Internet access for the first configure/build, including GitHub dependency
  downloads and submodules. A ZIP download of the game **still requires Git**.
- A graphical desktop for game clients. The server itself is headless.

### 2. Obtain the game

```sh
git clone --branch main --single-branch https://github.com/SuyeshJadhav/robot-factory-escape.git
cd robot-factory-escape
```

Alternatively, extract the game submission archive or
[download the main-branch ZIP](https://github.com/SuyeshJadhav/robot-factory-escape/archive/refs/heads/main.zip),
then open a terminal in the extracted directory containing this README,
`CMakeLists.txt`, and `assets/`. The game source already includes its sprites.

### 3. Configure and build

Run from the **game directory**, not from an engine directory. Use a fresh `build`
directory when changing compiler, generator, or source location.

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DGEF_ENGINE_DIR= \
  -DFETCHCONTENT_FULLY_DISCONNECTED=OFF \
  -DENGINE_BUILD_EXAMPLES=OFF \
  -DENGINE_BUILD_BENCH=OFF \
  -DGAME_BUILD_TESTS=OFF
cmake --build build --parallel 2
```

The first build may take several minutes. Both `build/robot_factory_escape` and
`build/robot_factory_server` must be produced. These commands intentionally omit
tests from the grader quick start; Python and Linux namespace tools are not
needed just to build and play the game.

Dependencies, including the engine's HUD font, are downloaded into `build/_deps/`.
**Keep the game source/assets and build directory in place after building.** The
executables use paths configured at build time; they are not standalone portable
binary distributions.

### 4. Run offline

From the same game directory:

```sh
./build/robot_factory_escape
```

A game window should open. Move with **A/D** or the arrow keys, jump with
**Space**, avoid hazards, and reach the exit. Close the window to quit.

## Multiplayer on one computer

Open a separate terminal in the game directory for each process. Start the server
first, then run the client command in two terminals to test two players (up to
four are supported). Use the **same mode and server port** in every process.
Close the clients and press **Ctrl+C** in the server terminal before switching
modes. The server normally has no graphical window.

### Client-server

**Terminal 1 — server:**
```sh
./build/robot_factory_server --mode client-server --bind tcp://127.0.0.1 --port 5555
```

**Terminals 2 and 3 — one client in each:**
```sh
./build/robot_factory_escape --mode client-server --server-host 127.0.0.1 --server-port 5555
```

### Hybrid peer-to-peer

This mode still uses a coordinator/shared-world server; it is not serverless.

**Terminal 1 — coordinator:**
```sh
./build/robot_factory_server --mode peer-to-peer --bind tcp://127.0.0.1 --port 5556
```

**Terminals 2 and 3 — one peer in each:**
```sh
./build/robot_factory_escape --mode peer-to-peer --server-host 127.0.0.1 --server-port 5556 --advertise-host 127.0.0.1
```

### What to check

- Move in one window; the other should show its remote robot moving.
- Die in one window; the other should hide the robot and show
  `STATUS: PLAYER ... DESTROYED`. Respawn should restore it and show `SPAWNED`.
- Record and play a replay in one client; the other must not receive the recorded
  actions as new live movement or death/spawn events.

## Multiplayer across two LAN machines

Build the **same game revision** on both machines and use a trusted LAN with
client-to-client communication enabled. Guest/campus Wi-Fi may isolate devices.
The following addresses are **examples**; replace them with the actual LAN IPv4
addresses. Do not use `127.0.0.1` for a service on another machine.

Assume machine A is `192.168.1.10`, machine B is `192.168.1.11`.

### Client-server across the LAN

**Machine A — server:**
```sh
./build/robot_factory_server --mode client-server --bind tcp://192.168.1.10 --port 5555
```

**Machine A and machine B — one client on each:**
```sh
./build/robot_factory_escape --mode client-server --server-host 192.168.1.10 --server-port 5555
```

### Hybrid peer-to-peer across the LAN

Stop the client-server processes first.

**Machine A — coordinator:**
```sh
./build/robot_factory_server --mode peer-to-peer --bind tcp://192.168.1.10 --port 5556
```

**Machine A — peer:**
```sh
./build/robot_factory_escape --mode peer-to-peer --server-host 192.168.1.10 --server-port 5556 --advertise-host 192.168.1.10
```

**Machine B — peer:**
```sh
./build/robot_factory_escape --mode peer-to-peer --server-host 192.168.1.10 --server-port 5556 --advertise-host 192.168.1.11
```

**Firewall note:** `5555`/`5556` are join ports, not the entire connection. The
engine also negotiates additional dynamically assigned TCP ports on the server;
peer-to-peer mode adds a dynamic TCP listener on each peer. Allow the game/server
processes to communicate only on the trusted private LAN as your OS permits.
Opening only the join port can allow joining but prevent gameplay updates. Do not
disable the firewall or expose this coursework service through router port
forwarding. No Internet/NAT traversal or secure public hosting is claimed.

## Controls and replay

| Key | Action |
| --- | --- |
| A / D or Left / Right | Move left / right |
| Space | Jump while grounded |
| T | Pause / resume your local simulation |
| 1 / 2 / 3 | Half / normal / double local speed |
| R | Restart your robot; also ends playback if active |
| P | Toggle display scaling (fixed during playback) |
| F5 | Start a new recording / stop recording |
| F6 | Start playback / stop playback |
| Esc | Stop playback and return to live gameplay; does not quit the game |
| Close window | Quit |

To demonstrate replay: press **F5**, move/jump or die and respawn, press **F5**
again, then **F6**. Playback ends naturally, or use **Esc/F6** to stop it early.
Unpause and let your robot respawn before starting playback. Recording retains a
rolling window of approximately **15 game-time seconds**, not unlimited history;
starting a new recording replaces the old one. Gameplay input and time controls
are blocked during playback. Pausing one client does not pause the server's shared
world or other clients.

## Optional: use the separately submitted team engine

Use a **complete v0.4.0-compatible** engine checkout, including
`examples/hello_entity/assets/SuperBouncer.ttf`. The v0.3.0 engine from Milestone 3
cannot supply Milestone 4's event/replay headers. There is no need to build the
engine separately or edit its CMake files.

For sibling folders `GEF_engine/` and `robot-factory-escape/`, run from the game:

```sh
cmake -S . -B build-team -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DGEF_ENGINE_DIR="../GEF_engine" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=OFF \
  -DENGINE_BUILD_EXAMPLES=OFF \
  -DENGINE_BUILD_BENCH=OFF \
  -DGAME_BUILD_TESTS=OFF
cmake --build build-team --parallel 2
./build-team/robot_factory_escape
```

Change the engine path if it is elsewhere. To return to automatic download,
configure a fresh build directory with `-DGEF_ENGINE_DIR=` as in the quick start.

## Optional: regression tests

Configure a separate test build so the play-only quick start stays simple:

```sh
cmake -S . -B build-tests -G Ninja -DCMAKE_BUILD_TYPE=Debug -DGEF_ENGINE_DIR= -DGAME_BUILD_TESTS=ON
cmake --build build-tests --parallel 2
ctest --test-dir build-tests --output-on-failure
```

Application smoke tests are registered only on **64-bit Linux/GNU** when Python 3,
`unshare`, and `ip` are found. They use isolated network namespaces and need
unprivileged user/network namespaces enabled; some sandboxes disallow this.
On such a host, `ctest --test-dir build-tests --output-on-failure -LE app` excludes
only those app-smoke tests, not the ordinary socket tests. This is reduced
coverage, not a full-suite pass. The tests do not replace separate-machine LAN
validation.

## Troubleshooting

- **Wrong working directory / executable not found:** run commands from the game
  source directory. Check that configuration and compilation both finished
  successfully, then check the two executable paths in step 3. Do not run an old
  binary copied from another machine or build folder.
- **Missing compiler, Ninja, or old CMake:** install/check the prerequisites before
  configuring. A generator/compiler mismatch needs a new build directory, not an
  edit to the game's CMake files.
- **Download or Git/submodule failure:** the initial build needs working access to
  GitHub. Resolve that network/Git error and reconfigure; do not set
  `FETCHCONTENT_FULLY_DISCONNECTED=ON` on an empty build directory.
- **Missing engine / event headers / HUD font:** use the default empty
  `GEF_ENGINE_DIR`, or supply the complete compatible engine checkout. Do not point
  it at the game, an old release, or just the engine's `include/` directory.
- **No window / `wayland not available`:** use a terminal in your graphical desktop,
  not a headless SSH session. Remove any inherited `SDL_VIDEODRIVER=dummy` test
  override. On a desktop with Xwayland, try
  `SDL_VIDEODRIVER=x11 ./build/robot_factory_escape`. If necessary, try
  `SDL_VIDEODRIVER=x11 SDL_RENDER_DRIVER=software ./build/robot_factory_escape`.
  These are per-launch fallbacks, not changes to desktop settings. Install desktop
  development dependencies before configuring; use a fresh build directory after
  adding a missing backend's dependencies.
- **No multiplayer connection:** check server/client modes and ports match, start
  the server first, and read its terminal for errors. For LAN, check the actual
  addresses, firewall rules, and dynamic-port note above; peer advertisements must
  contain each peer's own reachable LAN address.

## Verification scope

Debug and Release regression gates, real application headless probes, and
user-reported desktop checks of offline/replay and both local two-client modes
have passed on Linux. Separate-machine LAN, macOS, and Windows behavior remain
unverified. The team replay controller also has a known stored-sample timing
limitation when recorded ticks contain gaps; no engine timing fix is claimed here.

## Credits

- Requirements: CSC 481/581 Milestone 4 course brief.
- Engine: [GEF_engine](https://github.com/ShounakDeshmukh/GEF_engine), by The Deadlock
  Engine Authors, under its [MIT license](https://github.com/ShounakDeshmukh/GEF_engine/blob/603154b11de36bbf837605fa3da43d949b3f4a5f/LICENSE).
- Sprites: course-provided artwork from the [NC State DELTA CSC 481 sprite
  selector](https://issdev.delta.ncsu.edu/courses/csc481-sprite-selector/), stored in
  `assets/sprites/`. No redistribution permission beyond coursework is claimed.
- HUD font: [Super Bouncer by fsuarez913](https://www.dafont.com/super-bouncer.font),
  supplied through the engine's example assets; its terms are separate from the
  engine's code license.
- Supporting libraries: SDL3/image/ttf, GLM, fmt, spdlog, nlohmann/json, and
  cppzmq/ZeroMQ, fetched by the engine under their respective upstream licenses.
- AI assistance: Hermes Agent (Nous Research) assisted with planning,
  implementation, debugging, verification, and documentation.
