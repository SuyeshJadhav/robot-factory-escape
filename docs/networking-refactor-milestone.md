# Networking refactor milestone

## Assignment and outcome

> Update your game to include the new networking refactor.

**Implemented and verified against the stated requirement.** The production game
and server consume the updated engine networking implementation in both
client-server and hybrid peer-to-peer modes. This is not a second networking
layer or a test-only integration.

Validation used GEF_engine revision
`0ec1cba862c91d2ed19bf2cf9b890cd51a5dede3` (engine 0.3.0). CMake consumes a
sibling checkout through `GEF_ENGINE_DIR`; it does not automatically pin or
update that checkout. The fresh verification run used game revision
`80ceb88409319f906e8797098a889b701f832bd7`; subsequent milestone-record edits
are documentation-only.

## Task status

This follows the status/implementation/evidence format used for the earlier
milestones. It does not add learning-outcome mappings or requirements absent
from this week's assignment.

| Assignment task / integration check | Status | Implementation in this game / engine | Evidence / how to verify |
| --- | --- | --- | --- |
| Include the new networking refactor | Implemented; Debug build and regression tested | The game joins through `sessionClient` or `peerSession`; the headless server uses `sessionServer`; owned state and animation metadata pass through `sceneReplicator`. | `src/networking/game_network.cpp`, `src/server/main.cpp`, and `src/app/game_loop.cpp`; fresh configure/build against the engine revision above passed. |
| Consume the actual updated engine | Verified for the recorded checkout | Root CMake adds `GEF_ENGINE_DIR` and links the production game/server and tests to `engine::engine`. | `CMakeLists.txt`; confirm the sibling engine commit before configuring. No engine files were changed by this milestone. |
| Preserve client/server ownership | Implemented; regression tested | Each client owns its robot. The server/coordinator owns the shared drone and moving platform. Peer mode sends robot state directly while retaining the coordinator for roster and shared-world state. | `network_sessions` checks both modes, bidirectional replication and server-owned moving-platform delivery/carrying. |
| Remain compatible with snapshot representations | Regression tested | Production continues using default Binary snapshots. The consumer regression separately exercises the engine's Binary and JSON serializers and decoder. | `network_refactor` checks stable robot IDs, bound-entity reuse, robot position, local texture preservation, animation metadata, truncation rejection and owner cleanup. |
| Preserve pause, late join and departure behavior | Automated coverage; selected human checks reported | Networking continues while local physics is paused. Paused peers republish frozen state for late joiners; departed robots are removed. | `network_state` and `network_sessions`; human checks also reported mid-jump pause in both modes and successful paused late join. Full graphical sign-off is separate below. |

## Changes made for this milestone

The production game already used the session-based networking interfaces. The
required increment was to verify compatibility with the current engine refactor
and add a focused consumer regression, not redesign gameplay or duplicate the
engine transport.

| File | Change |
| --- | --- |
| `tests/network_refactor_tests.cpp` | Added a real Scene/replicator round trip for Binary and JSON, exact animation bytes, local texture preservation, truncated-packet rejection, remote physics exclusion and departed-owner cleanup. |
| `tests/CMakeLists.txt` | Registered `network_refactor` through the existing game-test helper, bringing the suite to eight tests. |
| `docs/multiplayer-verification.md` | Recorded the target engine revision, disconnected build procedure, actual automated/manual evidence, listener exposure and verification limits; added the independent-audit follow-up. |
| `docs/networking-refactor-milestone.md` | Added this assignment-to-implementation/evidence record in the earlier milestone-table format. |

Implementation and initial evidence commits:

- `d0d21f7a39963c6f4116e79546403a42fff5aef2` — compatibility regression and registration.
- `80ceb88409319f906e8797098a889b701f832bd7` — current engine integration and verification documentation.

No new interpolation, prediction, rollback, shared-winner system, or synchronized
round restart was added. Finishing and restarting remain local to each player,
as chosen for the course demonstration. Restart preserves the selected pause
and speed; T resumes a robot reset while paused. Speed controls scale local
simulation, including jump timing, rather than only walking speed.

## Verification results

A fresh disconnected Debug configure/build passed. Then:

| Check | Actual result |
| --- | --- |
| `network_state`, `network_refactor`, `network_sessions`, each repeated with `--repeat until-fail:5` | Five passes each; counts checked programmatically. |
| Full game CTest suite | 8/8 passed. |
| Three independent read-only reviews | No blocker found to the stated networking-refactor adoption task. Reviewed production integration, regression/CMake validity, and milestone/demo boundaries. |

Reproduction instructions, commands for both network modes and the graphical
checklist are in [Multiplayer verification](multiplayer-verification.md).
Use its exact-engine/disconnected-build procedure; do not assume an existing
build cache or a different engine revision reproduces these results.

## Limits and remaining demo checks

- **Course requirement versus graphical sign-off:** networking-refactor adoption
  is supported by current source and the passing build/tests. The earlier local
  plan's complete graphical checklist is stricter and is not fully signed off.
  No instructor approval or unseen rubric coverage is claimed.
- **Gold remote-player backing:** `src/graphics/game_render.cpp` deliberately
  draws a filled gold marker behind animated remote players. The earlier yellow
  rectangle observation alone is not proof of failed sprite replication.
  Explicitly confirm remote artwork and animation with robots separated in both
  modes before claiming complete graphical sign-off.
- Remaining graphical observations include unconfirmed control/scaling details,
  platform riding/detachment, continued world updates during independent pause,
  resume, normal window-close cleanup and visible finish/restart feedback. Human
  reports of selected successful checks are not treated as exhaustive proof.
- The existing `network_state` test places required operations inside `assert`.
  Builds defining `NDEBUG` remove those operations and checks, so this audit's
  passing results are **Debug-only**, not Release-test certification. The new
  compatibility test uses runtime checks instead. No unrelated test repair was
  included.
- Truncation coverage checks selected existing robot state and metadata for one
  truncated packet per representation, not exhaustive malformed inputs or
  complete scene transactional behavior. JSON is tested at replicator level;
  production sessions remain default Binary.
- Shared drone artwork may stop animating in a locally finished player's window
  even while its networked position continues changing. This presentation
  limitation is not an adoption blocker.
- Localhost session tests do not establish two-machine LAN, public-network,
  mixed-version or cross-architecture behavior. Internal engine listeners can
  bind all interfaces even when coordinator addresses use localhost; follow the
  verification guide's authorization prerequisite.

**Summary:** the game includes the updated networking refactor and has passing
Debug consumer/integration evidence. Optional presentation improvements and
remaining full graphical checks do not imply another networking implementation
is needed for this assignment.


## Complete individual-game Milestone 3 integration — 2026-10-21

This heading names the intended Part 2 scope, not a claim that every acceptance
gate has passed. The earlier sections record the narrower networking-refactor
adoption at their own revisions; those historical passes are not re-used as proof
for the new world.

Game implementation: `6d6bf6dfe8a12ee4e365cbebe81a714622533e79`. Engine:
`0ec1cba862c91d2ed19bf2cf9b890cd51a5dede3`. The following documentation commit
changes no tested source.

| Requirement | Current implementation | Evidence |
|---|---|---|
| Keyboard movement/jump | Existing `Game::handleInput`/`player_motion`; new shared tick | Headless movement/carry/gameplay tests PASS; real keyboard/window demonstration BLOCKED. |
| Distinct static platforms | Eight static platform specs in `src/gameplay/factory_world.cpp` | `factory_world` PASS. |
| Two motion patterns | `MovingPlatformPatrol` and `factory::Orbit` attached to scene entities | `factory_motion` and offline/client ownership assertions PASS. |
| Multiple hidden spawns | `SpawnPoint` entities and explicit local `RespawnTarget` | `factory_world`/`respawn` PASS. |
| Collision death zones | Hidden pit/perimeter collider entities, `inDeathZone` and `respawnPlayer` | `respawn`/`factory_gameplay` PASS; pit traversal uses actual physics. |
| Camera alternative | `graphics/camera.hpp` and offsets in `game_render.cpp` | `camera` PASS; visual HUD/door/marker/scale observations BLOCKED. |
| Endpoint object model | One renderer-independent world factory on server and clients; fixed network IDs | Binary/JSON `factory_replication` PASS; live four-client scenarios compiled but BLOCKED. |

No new ECS, transport, event system, configuration framework, or engine change.
The scene owns simple movement/spawn/death components; `Game` retains input/UI and
session coordination. Clients advance only their robot; offline ticks/server own
world motion. Camera rendering never rewrites simulation coordinates.

Replacement movement/restart tests passed before the old callers were removed.
The two legacy drone files remain untouched on disk (no deletion approval), but
are unused by production and no longer registered as a test target.

Actual acceptance: fresh Debug configure/build PASS, 12/12 non-socket tests PASS,
14 target names reconciled, both CLI help paths PASS. The new network test needed
two C++ dependent-template qualifiers to compile; it has not been executed.
Current socket tests, repeated full-suite runs, and all graphical demonstrations
are BLOCKED by approval boundaries. No latency, LAN, arbitrary-connection capacity,
full assignment completion, or independent final-review approval is claimed here.

Still required for submission: graphical and four-client evidence; sprite/font
attribution (see README); exact-revision source export and fresh exported-source
build/asset checks; team-engine delivery; applicable performance experiments; the
individual paper and appendix. No upload/submission occurred. Local graph artifacts
were not refreshed and remain stale.
