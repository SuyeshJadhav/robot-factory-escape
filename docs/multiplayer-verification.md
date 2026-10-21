# Multiplayer verification

The game uses the updated engine's `sessionServer`, `sessionClient`,
`peerSession`, and `sceneReplicator`. This replaced the earlier custom position
packet API, which no longer builds against the current engine.

## Current networking-refactor milestone

See the [milestone task and change record](networking-refactor-milestone.md)
for the assignment/status/evidence table and the changes made for this milestone.

Validate this game against GEF_engine revision
`0ec1cba862c91d2ed19bf2cf9b890cd51a5dede3` (engine 0.3.0).
The older engine checkout shown elsewhere in the README is not this milestone's
validation target. The engine is consumed from a sibling checkout, not vendored
or automatically pinned by the game's CMake project.

The game already uses `sessionServer`, `sessionClient`, `peerSession`, and
`sceneReplicator`. Each client owns its robot; the server owns the shared drone
and moving platform. Client-server mode relays robot state through the server;
peer mode exchanges robot state directly and uses the coordinator for roster
and shared-world updates. Animation metadata uses `setExtra`; texture handles
remain local. This is C++20/SDL3 integration, not C++17/SDL2.

Before any full-suite or networked application run, note that the engine's
per-client handlers and peer publishers bind `tcp://*:0`. A localhost
coordinator/advertised address does not make these other listeners loopback-only.
Obtain explicit authorization for this exposure or verify an existing isolated
environment first; do not change firewall settings or engine bindings here.
Only after recording that prerequisite, set `export RFE_NETWORK_TESTS_AUTHORIZED=1`
in each test/application terminal. The flag acknowledges the prerequisite; it
does not provide isolation, and plan approval alone does not authorize exposure.
Otherwise leave socket checks BLOCKED and do not run the full suite.

From the game root, with the engine checkout at the revision above and the
socket prerequisite satisfied:

```sh
test "${RFE_NETWORK_TESTS_AUTHORIZED:-}" = 1 &&
test "$(git -C ../GEF_engine rev-parse HEAD)" = 0ec1cba862c91d2ed19bf2cf9b890cd51a5dede3 &&
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DGEF_ENGINE_DIR="$PWD/../GEF_engine" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
  -DENGINE_BUILD_TESTS=OFF -DENGINE_BUILD_EXAMPLES=OFF \
  -DENGINE_BUILD_BENCH=OFF -DGAME_BUILD_TESTS=ON &&
cmake --build build --parallel 3 &&
ctest --test-dir build --no-tests=error --output-on-failure
```

Every command is gated on its predecessor's success; missing cached dependency
sources are a blocker requiring separate authorization, not permission to fetch.
The suite has eight CTest tests after the compatibility target is added.
`network_refactor` checks Binary/JSON compatibility, animation bytes, local
texture preservation, truncated Binary/JSON snapshot rejection, and remote cleanup.
The game itself continues to use the default Binary representation.

The commands and manual checklist are verification instructions, not evidence
that a run has passed. Record fresh results below before claiming completion.

### Milestone verification record

For each item, record the exact command or observation, actual result, date,
game commit, and engine commit. Use PASS, FAIL, or PENDING; do not call a result
PASS if it was not exercised. Use BLOCKED when a prerequisite is unavailable.
Report **automated compatibility verified** only after the fresh build, repeated
network tests, and full eight-test suite pass. Report **full milestone verification
complete** only after those automated gates and all three graphical checks pass.
Any PENDING/BLOCKED graphical check leaves full verification incomplete; optional
LAN play does not block these localhost-scoped completion gates.

Recorded at `2026-10-20T22:12:33-04:00` for game
`d0d21f7a39963c6f4116e79546403a42fff5aef2` and engine
`0ec1cba862c91d2ed19bf2cf9b890cd51a5dede3`. The later documentation-only
commit does not change the tested binaries. The user explicitly authorized the
temporary test/application listeners before execution. No firewall changes,
dependency downloads, engine edits, or public-network tests were performed.

| Check | Actual result |
|---|---|
| Fresh configure/build | PASS: the disconnected configure/build commands above compiled the game, server, and tests against the specified engine. |
| Baseline networking | PASS: `ctest --test-dir build -R '^network_(state|sessions)$' --no-tests=error --output-on-failure`, 2/2 tests. |
| New compatibility target | PASS: `ctest --test-dir build -R '^network_refactor$' --no-tests=error --output-on-failure -V`, 1/1; output included `Networking refactor compatibility checks passed.` |
| Repeated networking | PASS: `ctest --test-dir build -R '^network_(state|refactor|sessions)$' --repeat until-fail:5 --no-tests=error --output-on-failure`; each of three registered tests passed five times. |
| Full game suite | PASS: `ctest --test-dir build --no-tests=error --output-on-failure`, 8/8 tests, 14.33 seconds in the full run after repetitions. |
| Offline graphical checklist | PENDING: the real app rendered the level, robot, drone, platform, and HUD. Movement/jump, T pause/resume, paused reset, scale, speed-key, and platform-riding sign-off remain unverified. |
| Client-server graphical checklist | FAIL visual check / remaining interaction PENDING: two real graphical clients joined as IDs 1 and 2 at 1x/0.5x. A solid yellow rectangular overlay appeared behind the local robot at the common spawn while both clients were present, then disappeared after client 1 left. Correct remote animated-sprite appearance is not signed off. Pause/control/platform-riding observations remain PENDING. |
| Peer graphical checklist | FAIL visual check / remaining interaction PENDING: two real graphical peers joined as IDs 1 and 2, with the same yellow remote-player overlay; it disappeared from peer 2 when peer 1 left. Paused late join, movement, pause, and platform-riding sign-off remain PENDING. |
| Two-machine LAN play | PENDING / not performed; optional and not established by localhost tests. |

The missing-target RED was `ninja: error: unknown target 'network_refactor_tests'`
(exit 1) before CMake registration. This demonstrates missing registration, not
a behavioral engine defect; the existing behavior passed once the test was registered.

Application commands actually exercised (one command per terminal, after the
listener prerequisite and `export RFE_NETWORK_TESTS_AUTHORIZED=1` in each):

```sh
# Offline:
./build/robot_factory_escape
# Client-server server and clients:
test "$RFE_NETWORK_TESTS_AUTHORIZED" = 1 &&
./build/robot_factory_server --mode client-server --bind tcp://127.0.0.1 \
  --port 5555 --stats-interval-ms 1000
test "$RFE_NETWORK_TESTS_AUTHORIZED" = 1 &&
./build/robot_factory_escape --mode client-server --server-host 127.0.0.1 \
  --server-port 5555 --initial-speed 1
test "$RFE_NETWORK_TESTS_AUTHORIZED" = 1 &&
./build/robot_factory_escape --mode client-server --server-host 127.0.0.1 \
  --server-port 5555 --initial-speed 0.5
# Peer coordinator and clients (first peer joined before starting the second):
test "$RFE_NETWORK_TESTS_AUTHORIZED" = 1 &&
./build/robot_factory_server --mode peer-to-peer --bind tcp://127.0.0.1 \
  --port 5556 --stats-interval-ms 1000
test "$RFE_NETWORK_TESTS_AUTHORIZED" = 1 &&
./build/robot_factory_escape --mode peer-to-peer --server-host 127.0.0.1 \
  --server-port 5556 --advertise-host 127.0.0.1
test "$RFE_NETWORK_TESTS_AUTHORIZED" = 1 &&
./build/robot_factory_escape --mode peer-to-peer --server-host 127.0.0.1 \
  --server-port 5556 --advertise-host 127.0.0.1 --initial-speed 0.5
```

Both binaries' `--help` commands exited 0 and listed both network modes; the
client also listed `--initial-speed 0.5|1|2`. Every graphical client logged
`Joined as client` and `SimulationThread starting`; both servers logged their
mode's `Headless ... server listening` message. Both client-server HUDs showed
their distinct connected IDs and requested speeds; peer 2's HUD showed
`PEER PLAYER 2 CONNECTED` and `RUNNING 0.5X`.

Client-server server counters `(updates, state, snapshots)` at 22:08:14 were
client 1 `(1084, 1083, 1079)` and client 2 `(537, 536, 537)`; at 22:08:19 they
were `(1386, 1385, 1378)` and `(687, 686, 687)`. In peer mode, at 22:11:47 they
were client 1 `(6804, 0, 6775)` and client 2 `(2010, 0, 2009)`; at 22:11:51
they were `(6984, 0, 6955)` and `(2100, 0, 2099)`. Peer `state=0` is expected:
robots publish directly, not through coordinator state submission. Growing
coordinator counters prove world refresh, not direct peer robot delivery;
that delivery is covered by the automated session assertions. The shared
platform/drone rendered at changing positions in each graphical mode.

All five graphical game processes and both server processes exited 0 after
SIGINT shutdown requests. These were not verified close-button interactions.
Background and explicitly authorized foreground computer-use key events, plus
a native X11 key attempt, did not confirm T/pause. Input delivery was unverified
in this run; this does not establish failure of a human's physical keyboard.
No game or engine source was changed to work around it.

The yellow remote-player overlay was observed in both modes and disappeared
after departure; that is narrower than proving correct remote sprite animation.
Its root cause and whether it predates the engine update are unverified.
Investigate it separately before graphical sign-off; this test/document-only
change does not repair rendering.

**Outcome: automated compatibility verified; full milestone verification incomplete.**

### Follow-up independent audit

Three independent read-only reviews found no blocker to the supplied course
requirement, "Update your game to include the new networking refactor." They
checked production integration, regression/CMake coverage, and demo boundaries.
The game already consumes the session APIs in production; the contribution adds
focused compatibility coverage and evidence rather than another transport layer.
A fresh disconnected Debug configure/build passed, each focused networking test
passed five repetitions, and the full suite passed 8/8. This establishes the
stated adoption task, not every graphical gate in the earlier local checklist.

The yellow backing needs a narrower interpretation than the original visual
record: `src/graphics/game_render.cpp` intentionally draws a filled gold marker
behind animated remote players. A rectangle can therefore coexist with working
remote sprite animation; its presence alone does not prove replication failure.
The original observation remains historical evidence, not an established
networking-regression diagnosis. Confirm the artwork with separated robots in
both modes before complete graphical sign-off.

Human checks subsequently reported working speed controls (including slower
jump timing), mid-jump pause in both modes, and paused late join. Restarting
while paused preserves pause and leaves the robot at spawn until T resumes;
that is the intended contract. Independent finishes and local restarts remain
the selected demo behavior; no shared-winner system was added. Remaining manual
checklist items are not inferred to pass from a general report that things work.

The audit also identified an existing test-harness limit: `network_state` puts
required `track`/`apply` operations inside `assert`, so it is not safe evidence
for builds defining `NDEBUG`. These passing results are Debug-only. The new
`network_refactor` uses runtime checks. The truncation fixture tests selected
existing state, not complete scene atomicity or arbitrary malformed packets.
Shared drone animation can also freeze after local victory while its received
position keeps updating; that presentation limitation is not a refactor-adoption
blocker. These findings were not silently expanded into unrelated repairs.

## Automated checks (historical networking-only baseline)

From the game directory:

```sh
cmake --build build
ctest --test-dir build --output-on-failure
```

That networking-only baseline had eight tests. `network_state` checks network IDs,
versioned animation metadata, remote entity physics exclusion, and local
pause/speed behavior using a controlled clock. `network_sessions` starts a
real server and two clients on localhost in both modes, checks bidirectional
robot replication, a server-owned moving platform carrying both clients' robots,
a late-joining peer receiving a frozen robot, and robot
removal after departure. `network_refactor` additionally checks both engine
snapshot representations, exact game animation bytes, local texture preservation,
malformed snapshot rejection, and owner cleanup. The remaining tests cover movement, drone patrol,
progress, and sprites. `moving_platform` covers riding through reversals, walking
and jumping off, delayed updates, pause/resume support, and wall collisions.

## Manual demonstration

Use the commands in the README to start a server and two graphical clients.
Check that each client has a different ID, sees the other animated robot,
shared drone, and cyan-outlined moving platform, and that pressing `T` or `1/2/3` in one window affects only that
window's robot. Closing one window should remove its robot from the other.
Repeat in peer-to-peer mode, including joining a second client while the
first is paused.

A headless SDL smoke run with two client processes confirmed that both could
join the server and start their simulation threads. The automated session
suite is the repeatable replication check; a human should still verify visual
sprite placement and control feel in a real window.

## Moving platform regression check

1. Offline, walk right and jump onto the cyan-outlined first platform. Release
   movement: the robot should keep the same position relative to its deck,
   including when it reverses direction.
2. Walk along the deck and jump off. The platform must not pull an airborne robot.
3. Repeat with two clients in each network mode. Each locally controlled robot
   should ride the shared platform, and the other client should see that movement.
4. Try 0.5x and 2x: the shared platform carries at the server's speed, independently
   of the local physics rate.
5. Pause while riding: the robot stays frozen while the networked platform moves.
   Resume: there is no accumulated platform jump. If the deck moved out from
   underneath, the robot falls and cannot jump in midair.
6. Press R while paused. The robot returns to its assigned spawn immediately;
   the selected pause and speed remain in effect. T resumes gameplay.

## Pause smoothness

Run two clients in each mode. Observe the FPS readout and the shared platform,
then press T in one window. The paused robot should freeze while shared objects
and the other player's robot continue updating. Both windows target 60 render
FPS, subject to hardware load. World refresh requests continue while paused;
client-server clients request snapshots without submitting new movement, and
peers republish their unchanged robot while requesting the shared world.

The FPS counter measures presentation frequency, independently of game time.
A low counter indicates rendering or machine load; a high counter with stepping
objects indicates sparse network updates. These are different measurements.


## Individual-game integration verification — 2026-10-21

Tested game source: `6d6bf6dfe8a12ee4e365cbebe81a714622533e79`; engine:
`0ec1cba862c91d2ed19bf2cf9b890cd51a5dede3`. Documentation is recorded afterward.
The previous sections are historical evidence for the earlier networking-only
revision, not acceptance of this larger world.

| Check actually performed | Result |
|---|---|
| Disconnected Debug configure/build, client + server + test binaries | PASS, using the nine existing dependency-source overrides below. No downloads or engine changes. |
| `ctest --test-dir build/milestone3 -E '^(network_sessions\|factory_network_sessions)$' --no-tests=error --output-on-failure` | PASS: 12/12 non-socket tests. Sprite tests use the registered dummy SDL driver. |
| `ctest --test-dir build/milestone3 --show-only=json-v1` | PASS: 14 distinct names reconciled against `tests/CMakeLists.txt` and the plan. |
| `build/milestone3/robot_factory_server --help` and `build/milestone3/robot_factory_escape --help` | PASS: both exit 0 before creating listeners or game windows. |
| `network_sessions`, `factory_network_sessions`, repeated socket checks and complete suite | BLOCKED: no listener authorization in this implementation run. Both targets compile; no runtime pass is claimed. |
| Offline full route, controls, hidden objects, HUD/door/camera/scaling | BLOCKED: no game-window launch authorization. Headless logic tests do not certify these observations. |
| Four simultaneous graphical clients, each driving a unique robot, in both modes | BLOCKED: no listener/window authorization. Must observe real keyboard movement/jump and remote rendering, plus pause, late join, death, finish/restart and normal disconnect. |
| Exact-revision source export, fresh exported-source build and relocated assets | PENDING: not performed. |
| Asset attribution | BLOCKED pending original sprite/font provenance and license evidence; see README inventory. |

Registered targets: `player_motion`, `moving_platform`, `game_progress`,
`game_sprites`, `network_state`, `network_refactor`, `network_sessions`,
`factory_world`, `factory_motion`, `respawn`, `factory_gameplay`, `camera`,
`factory_replication`, `factory_network_sessions`.

`factory_gameplay` tests positive-duration offline carry/mover advancement,
client non-advancement of shared objects, actual collision respawn, restart/re-win,
and pit traversal. `factory_replication` covers Binary and JSON without sockets,
including differing local IDs, hidden-component preservation and departed-owner
cleanup. `factory_network_sessions` assigns positions directly; even a future
pass will not measure input-to-screen latency or replace the keyboard demonstration.

The initial new API tests failed before implementation because their header/API or
motion definitions were absent; these are API REDs, not runtime defect claims.
The four-client test initially failed compilation on two missing dependent-template
qualifiers; both were fixed and the binary now builds. Its behavior remains unrun.
A baseline third-party SDL3_image build emitted a `strstr` macro-redefinition
warning; no engine/dependency edits were made to suppress it.

### Reproduce the authorized non-socket checks

Run from the game root with the pinned engine and already-populated sources.
A missing dependency, including SDL_ttf vendored dependencies, is a blocker, not
permission to download. This is Bash because the source overrides use an array.

```bash
export RFE_BUILD="$PWD/build/milestone3"
export RFE_CACHE="$PWD/build/_deps"
(
  set -e
  test "$(git -C ../GEF_engine rev-parse HEAD)" = 0ec1cba862c91d2ed19bf2cf9b890cd51a5dede3
  args=()
  for dep in sdl3 sdl3_image sdl3_ttf glm fmt spdlog nlohmann_json cppzmq libzmq; do
    test -f "$RFE_CACHE/$dep-src/CMakeLists.txt"
    args+=("-DFETCHCONTENT_SOURCE_DIR_${dep^^}=$RFE_CACHE/$dep-src")
  done
  cmake -S . -B "$RFE_BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug -DGEF_ENGINE_DIR="$PWD/../GEF_engine" \
    -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
    -DENGINE_BUILD_TESTS=OFF -DENGINE_BUILD_EXAMPLES=OFF \
    -DENGINE_BUILD_BENCH=OFF -DGAME_BUILD_TESTS=ON "${args[@]}"
  cmake --build "$RFE_BUILD" --parallel 3
  ctest --test-dir "$RFE_BUILD" -E '^(network_sessions|factory_network_sessions)$' \
    --no-tests=error --output-on-failure
  "$RFE_BUILD/robot_factory_server" --help
  "$RFE_BUILD/robot_factory_escape" --help
)
```

### Remaining acceptance gates

Only after explicit temporary-listener permission, run both socket targets five
times and then the full suite. The authorization variable acknowledges permission;
it does not restrict engine `tcp://*:0` bindings.

```bash
test "${RFE_NETWORK_TESTS_AUTHORIZED:-}" = 1 &&
ctest --test-dir "$RFE_BUILD" -R '^(network_sessions|factory_network_sessions)$' \
  --repeat until-fail:5 --no-tests=error --output-on-failure &&
ctest --test-dir "$RFE_BUILD" --no-tests=error --output-on-failure
```

After graphical approval, exercise the entire raised route, pit/perimeter deaths,
platform carry/detachment, camera/HUD and P scaling, pause/speed, win/R and paused R.
For each network mode, use four windows and real keyboard input in **each** window;
confirm every other client renders that robot, shared movers continue, cameras
remain local, a fourth client can join while another is paused, and normal closing
removes only that robot. Record observations individually; unconfirmed input
injection is not evidence of correct keyboard behavior. No <100 ms result is claimed.

Legacy `drone_patrol.hpp` and its test file are retained unchanged pending deletion
approval; active callers and the old CTest registration are gone. The graph is
stale because refresh was not authorized. Team-engine submission, applicable
benchmark experiments, the individual paper/appendix, attribution, packaging and
actual submission remain separate obligations. **Full milestone acceptance is
incomplete; the authorized implementation and non-socket verification are recorded.**
