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

The game suite currently has six tests. `network_state` checks network IDs,
versioned animation metadata, remote entity physics exclusion, and local
pause/speed behavior using a controlled clock. `network_sessions` starts a
real server and two clients on localhost in both modes, checks bidirectional
robot replication, a server-owned moving platform reaching both clients,
a late-joining peer receiving a frozen robot, and robot
removal after departure. The remaining tests cover movement, drone patrol,
progress, and sprites.

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
