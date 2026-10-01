# Multiplayer verification

The game uses the updated engine's `sessionServer`, `sessionClient`,
`peerSession`, and `sceneReplicator`. This replaced the earlier custom position
packet API, which no longer builds against the current engine.

## Automated checks

From the game directory:

```sh
cmake --build build
ctest --test-dir build --output-on-failure
```

The game suite currently has seven tests. `network_state` checks network IDs,
versioned animation metadata, remote entity physics exclusion, and local
pause/speed behavior using a controlled clock. `network_sessions` starts a
real server and two clients on localhost in both modes, checks bidirectional
robot replication, a server-owned moving platform carrying both clients' robots,
a late-joining peer receiving a frozen robot, and robot
removal after departure. The remaining tests cover movement, drone patrol,
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
6. Press R while paused. The robot returns to the common spawn immediately;
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
