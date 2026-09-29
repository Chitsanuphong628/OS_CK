# OS_CK — Concurrent Cinema Seat Reservation System

CSS223 OS Project (2569). A concurrent reservation system written in C++ that lets multiple clients reserve/cancel/check cinema seats through a shared server, communicating over a **System V message queue**, with a deliberately-triggerable race condition and a mutex-based fix.

**Scenario:** cinema seat booking. Each `resource_id` (1–20) represents one seat number; "reserving a resource" = booking that seat, "the owner" = the client who booked it.

This file is written to be readable on its own — by a teammate, a grader, or an AI picking up the project cold — without needing prior chat context.

---

## 1. Project structure

```
OS_CK/
├── common.h              Shared contract: message queue key, command/status enums, MessageBuffer struct
├── server.cpp            Server: owns the resource table, spawns workers, handles all commands
├── client.cpp            Client: interactive command-line program, one process per user
├── Dockerfile            Builds a single Ubuntu 22.04 image with both binaries compiled in
├── Makefile              Compiles server.cpp -> server_bin and client.cpp -> client_bin
├── LICENSE
├── README.md             This file
└── experiment/
    ├── exp1.sh           Sequential baseline (1 worker, sync off) — run locally with g++ (e.g. in WSL)
    ├── exp2.sh           Concurrent, no sync (3 workers) — reproduces the race condition
    ├── exp3.sh           Concurrent, with sync (3 workers + mutex) — shows the race is fixed
    └── logs/
        ├── exp1/ exp2/ exp3/   Output of the exp*.sh scripts above (compiled with plain g++, not Docker)
        └── docker/             Same 3 experiments plus a mixed-command run, executed inside the
                                 actual css223-reservation Docker image (see §7) — this is the
                                 "official" evidence, since the assignment requires the deliverable
                                 to run on Docker specifically, not just compile locally
```

`common.h` is the single source of truth for the wire format — if you change it, `server.cpp` and `client.cpp` both need rebuilding.

---

## 2. Message Queue design

One System V message queue, created with a fixed key so unrelated processes can find it:

```c
#define QUEUE_KEY        0x123456
#define SERVER_MSG_TYPE  1000
```

- **Requests** (client → server): every client sends with `mtype = SERVER_MSG_TYPE`. All 3+ worker threads call `msgrcv(..., SERVER_MSG_TYPE, 0)` on the *same* queue — whichever worker is currently idle receives the next message; the kernel does the load-balancing, there is no separate dispatcher.
- **Responses** (server → client): the worker that handled the request replies with `mtype = client_id`. Each client filters its own `msgrcv(..., client_id, 0)`, so it only ever receives its own response even though every client shares one queue.
- This is why **client IDs must be in the range 1–999** (`client_id < SERVER_MSG_TYPE`) — a client ID could otherwise collide with the reserved request-type value 1000.

Message layout (`common.h`):

```c
struct MessageBuffer {
    long mtype;
    int  client_id;
    int  command;       // CMD_LIST / CMD_STATUS / CMD_RESERVE / CMD_CANCEL / CMD_QUIT
    int  resource_id;   // seat number 1-20, unused for LIST/QUIT
    char payload[512];  // response text (unused on the request side)
};
```

The same struct is reused for both directions — a request only fills `client_id`/`command`/`resource_id`; a response additionally fills `payload`, and echoes back `client_id`/`command`/`resource_id` so the client can double-check the response actually matches what it asked (`client.cpp` verifies this before trusting `payload`).

---

## 3. Build

Requires Docker Desktop running.

```bash
docker build -t css223-reservation .
```

This installs `build-essential`, `make`, and `util-linux` (for `ipcs`/`ipcrm`, useful if a message queue ever needs to be inspected/cleared by hand), then runs `make` to produce `server_bin` and `client_bin` inside the image.

## 4. Run the container

Start one container and leave it running — every "terminal" in the demo is another shell into this *same* container, not a separate one:

```bash
docker run -d --name css223 css223-reservation
```

Single container is required, not just convenient: System V message queues live in the container's IPC namespace. Two separate containers would each create their *own* queue with the same key and never see each other.

## 5. Open the server

```bash
docker exec -it css223 bash
./server_bin <num_workers> <sync>
# e.g.
./server_bin 3 1
```

- `num_workers` (default 3 if omitted): how many worker threads process requests concurrently.
- `sync` (default 0/off if omitted): `1` = mutex protection enabled, `0` = disabled (used to deliberately reproduce the race condition — see §7).

On startup the server also checks for and removes any message queue left behind by a previous run that didn't shut down cleanly (e.g. `kill -9`), so it never starts by silently reusing a stale queue full of old messages.

Stop the server with `Ctrl+C` (or `docker exec css223 kill -INT <pid>` from another terminal) — this runs the cleanup handler, which removes the message queue via `msgctl(..., IPC_RMID, ...)`.

## 6. Open multiple clients

Repeat for each client, in its own terminal, against the *same* running container:

```bash
docker exec -it css223 bash
./client_bin <client_id>
# e.g. ./client_bin 1   (then 2, 3, 4, 5 in other terminals)
```

Each client needs a unique ID from 1–999. Once connected, type commands at the `Client-N>` prompt.

### Supported commands

| Command | Effect |
|---|---|
| `LIST` | Show all 20 seats and their status (`A` = available, `R` = reserved) |
| `STATUS <id>` | Show one seat's status (`AVAILABLE` or `RESERVED by Client-X`) |
| `RESERVE <id>` | Book seat `<id>` for this client |
| `CANCEL <id>` | Release seat `<id>` — only the client who booked it can cancel it |
| `QUIT` | Disconnect this client (the server keeps running for everyone else) |

---

## 7. Race condition experiments

The assignment requires reproducing the race condition on purpose, then showing the mutex fixes it. `handle_reserve()` in `server.cpp` does exactly the vulnerable sequence the spec describes:

```cpp
if (resource.status == AVAILABLE) {
    random_delay();               // 50-500ms, deliberately widens the race window
    resource.status = RESERVED;
    resource.owner_client_id = client_id;
}
```

`table_mutex.lock()`/`unlock()` wraps this block only when synchronization is enabled — same delay either way, so the *only* variable being tested is whether the check-then-write is atomic or not.

### Running the 3 required experiments

Two equivalent ways to run them — pick whichever matches what you need:

**A. Via the exp*.sh scripts (`experiment/` folder)** — compiles with plain `g++` (needs a Linux shell, e.g. WSL, with `g++` installed) and starts 5 clients that all send `RESERVE 10` at nearly the same instant:

```bash
cd experiment
./exp1.sh   # 1 worker,  sync off  -> Sequential Baseline
./exp2.sh   # 3 workers, sync off  -> reproduces the race condition
./exp3.sh   # 3 workers, sync on   -> shows the mutex fixes it
```

Logs land in `experiment/logs/exp{1,2,3}/`.

**B. Directly against the Docker image** (this is what produced `experiment/logs/docker/`, the official evidence, since the deliverable must run on Docker):

```bash
docker run -d --name css223-test css223-reservation
docker exec css223-test bash -c '
  ./server_bin 3 0 > server.log 2>&1 &
  SPID=$!; sleep 1
  for i in 1 2 3 4 5; do ( printf "RESERVE 10\nQUIT\n" | ./client_bin $i ) & done
  wait
  kill -INT $SPID
'
```

(swap `3 0` for `1 0` = Experiment 1, or `3 1` = Experiment 3)

### Turning synchronization on/off

Purely the second argument to `server_bin`: `0` = off (Experiment 2), `1` = on (Experiment 3/normal operation). Nothing else changes between runs — same worker count, same random delay — so it isolates exactly what the mutex is responsible for.

---

## 8. Results

All three experiments were run twice — once via the WSL scripts, once directly inside the Docker image — and gave matching outcomes both times. Full logs are in `experiment/logs/`; summary:

| Experiment | Workers | Sync | Result | What it proves |
|---|---|---|---|---|
| 1 — Sequential Baseline | 1 | off | Exactly 1 SUCCESS, 4 FAILED | No race is possible with only one worker, even unsynchronized |
| 2 — Concurrent, no sync | 3 | off | **3 clients got SUCCESS on the same seat** | Real race condition: all 3 workers read `AVAILABLE` before any of them wrote back |
| 3 — Concurrent, with mutex | 3 | on | Exactly 1 SUCCESS, 4 FAILED | Mutex serializes check-then-write; same delay as Exp2, but now no race |

Experiment 2's server log (`experiment/logs/docker/exp2.log`) shows the race directly — three consecutive `check Resource 10: AVAILABLE` lines from three different workers, before any of them had written a result:

```
[#1] [Worker-1] received RESERVE 10 from Client-3
[#2] [Worker-1] check Resource 10: AVAILABLE
[#3] [Worker-2] received RESERVE 10 from Client-1
[#4] [Worker-2] check Resource 10: AVAILABLE
[#5] [Worker-3] received RESERVE 10 from Client-2
[#6] [Worker-3] check Resource 10: AVAILABLE
[#7] [Worker-1] Resource 10 reserved by Client-3     <- first write only now
```

Experiment 3's log shows the same delay but with `entering/leaving critical section` markers: workers 2 and 3 receive their requests early but cannot enter the critical section until worker 1 leaves it, so they always see the up-to-date `RESERVED` state instead of a stale `AVAILABLE` one.

`experiment/logs/docker/demo1_mixed.log` additionally exercises all 4 commands (`LIST`, `RESERVE`, `STATUS`, `CANCEL`) from 5 different clients at the same time (the assignment's Demo 1 scenario) — no crash, every result consistent with the true final state, including the ownership check correctly rejecting a `CANCEL` from a client that doesn't own that seat.

Every log line is tagged with a `[#N]` sequence number (a global atomic counter, incremented once per printed line) — this is the log's answer to the spec's "approximate time or sequence number" requirement, and gives a single, unambiguous ordering of events across all worker threads for analysis.

### Known, deliberate design notes

- `STATUS`/`LIST` always take `table_mutex` regardless of the `sync` flag (independent of the experiment toggle) — reads should never race a write, and since none of the 3 required experiments send `STATUS`/`LIST` concurrently with `RESERVE`, this doesn't affect any experiment's result.
- The server checks for and removes a leftover message queue on every startup (see §5) so a prior crash can never leave stale messages for the next run to accidentally consume.

---

## 9. Scalability / timing (not required by the spec — extra depth for the report's limitations section)

Full log: `experiment/logs/docker/scalability.log`. Measured with `date +%s%N` around a batch of concurrent clients inside the Docker image, `sync=ON` throughout.

**Does adding workers speed up RESERVE?** 20 clients each reserving a distinct seat, worker count varied:

| Workers | Clients | Elapsed |
|---|---|---|
| 1 | 20 | 5817 ms |
| 3 | 20 | 6398 ms |
| 10 | 20 | 6180 ms |

Barely any difference. `table_mutex` is one global lock over the whole seat table, and `random_delay()` (50–500ms) runs *inside* that lock in `handle_reserve()` — every successful reservation, regardless of which seat, must wait for the current lock-holder's full delay. With 20 reservations averaging ~275ms each, total time is bounded below by ~5.5s no matter how many worker threads exist, because they all queue on the same mutex. Worker count only helps once there's something to actually run in parallel — inside `handle_reserve` there isn't.

**Does client load affect read-only STATUS throughput?** Workers fixed at 3, client count varied, each sending one `STATUS`:

| Clients | Elapsed |
|---|---|
| 5 | 5 ms |
| 20 | 10 ms |
| 50 | 20 ms |
| 200 | 57 ms |

Scales close to linearly, no breakdown observed up to 200 concurrent clients — `STATUS`'s critical section is a map lookup with no delay, so the lock is held for microseconds rather than hundreds of milliseconds.

**Take-away:** this system's throughput ceiling comes from the single global mutex + the delay held inside it while reserving, not from worker count or message-queue capacity (kernel default queue size is 16 KB, comfortably more than what these tests generated). A per-resource lock instead of one table-wide lock would let reservations on different seats run fully in parallel — a reasonable future improvement, and a reasonable trade-off to have skipped for a 20-seat/5-client assignment.

*Methodology note: each "client" is a full OS process spawned via `docker exec`; at much higher counts the process-spawn overhead itself would start to dominate the measurement, so treat these as illustrative of the server's actual bottleneck, not as a rigorous load-testing tool's output.*
