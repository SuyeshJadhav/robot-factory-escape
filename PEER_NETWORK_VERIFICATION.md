# Section 5 peer networking verification

## Result

The hybrid model is implemented. A dedicated headless coordinator assigns peer
IDs and endpoints and supplies the shared drone position. Every game process
publishes its own player position to the other processes over a direct ZeroMQ
PUB/SUB connection. The coordinator handles registration and world requests; it
has no code path that accepts or forwards a `PlayerState` packet.

| Assignment behavior | Evidence | Result |
| --- | --- | --- |
| Multiple independent client processes | Launched concurrent `robot_factory_escape` instances against `robot_factory_server`; assigned distinct IDs (e.g. 1 and 2) | Pass |
| Direct peer player data | Each client's log reported receiving positions directly from the other peer via ZeroMQ PUB/SUB | Pass |
| Late joining | Newly joined clients connect to existing peers and exchange state updates | Pass |
| Departure | Remaining clients log remote robot entity removal upon peer disconnection / departure | Pass |
| Shared moving object | Headless coordinator advances and publishes 60 Hz drone position & tick; clients sync positions within < 250 ms latency | Pass |
| Simulation threading & rendering | Physics & state run on `SimulationThread`, rendering on main thread, sockets on network workers | Pass |
| Timeline pause & speed scaling | Keyboard `T` pauses local simulation without dropping network or remote updates; `1/2/3` scales speed (0.5×, 1×, 2×); `P` toggles scaling | Pass |

All 118 engine CTest test cases passed (100%), including `network_three_processes`.
All 5 game CTest test cases passed (100%), including `network_state`.

## Reproduce

1. Build the engine and game:
   ```bash
   cmake --build build
   ```
2. Run test suites:
   ```bash
   ctest --test-dir build --output-on-failure
   ```
3. Run coordinator server:
   ```bash
   ./build/robot_factory_server --port 5678 --bind tcp://*
   ```
4. Run multiple game clients from separate terminals:
   ```bash
   ./build/robot_factory_escape --server-host 127.0.0.1 --server-port 5678 --advertise-host 127.0.0.1
   ```
   - Control movement with `A`/`D` and `Space`.
   - Observe remote player robots in real time.
   - Press `T` to pause local simulation (remote players and drone continue updating; heartbeat remains alive).
   - Press `1`, `2`, `3` to scale simulation speed to 0.5×, 1×, or 2×.
   - Press `P` to toggle window scaling between proportional and constant.
   - Disconnecting one client removes its robot on other clients cleanly.

