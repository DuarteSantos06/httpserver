# HttpServer

An asynchronous HTTP/1.1 server written in C from scratch, using `epoll`, non-blocking sockets and a multi-threaded worker model, with no external frameworks.
This project was developed as a learning exercise to demonstrate my knowledge of systems programming.

## Features

- Handles HTTP `GET` and `POST` requests, with keep-alive support
- Serves static files from the `www/` directory with zero-copy `sendfile()` (automatically detecting formats like `.html`)
- Routes requests dynamically based on the HTTP method and path
- Provides predefined routes such as `/status` (JSON state) and `/data` (file persistence)
- Per-IP token-bucket rate limiting on new connections (`429 Too Many Requests`)
- Closes idle connections after 15 seconds
- Rejects path traversal attempts and records them in an audit log
- Responds with `400 Bad Request` if the HTTP syntax or path is invalid
- Responds with `404 Not Found` if no route or file matches the request
- Responds with `413 Payload Too Large` if the request exceeds 4 KB
- Features a modular design, making it easy to add new routes and custom handlers

## Architecture

```
                      kernel (SO_REUSEPORT)
          ┌──────────────┬───────┴──────┬──────────────┐
          ▼              ▼              ▼              ▼
     listen socket  listen socket  listen socket  listen socket
          │              │              │              │
       worker 0       worker 1       worker 2       worker 3
     (own epoll)    (own epoll)    (own epoll)    (own epoll)
```

- **One listening socket per worker.** Every worker binds its own socket to port 8080 with `SO_REUSEPORT`, and the kernel hashes each new connection (source/destination IP and port) to one of them. This spreads connections evenly across workers, instead of letting the first worker to wake up drain a shared accept queue.
- **One `epoll` instance per worker.** A connection lives on the same worker for its whole lifetime, so workers share no connection state and need no locks on the hot path.
- **Non-blocking state machine.** Each client moves between `READING` and `WRITING`; partial reads and writes are resumed on the next `epoll` event.
- **Idle timeout.** Each worker keeps its clients in a list ordered by last activity, so idle connections are found and closed by walking only from the head of the list.
- **Rate limiting.** A 256-shard hash table (one mutex per shard) stores a token bucket per IP, with a background thread removing stale entries. Localhost is exempt so benchmarks are not throttled; build with `make NO_RATE_LIMIT=1` to disable it entirely (e.g. for load tests from a single remote IP).


The number of workers is set by `WORKER_COUNT` in `include/server.h`.

## Project structure
```
HttpServer/
│
├── src/
│   ├── main.c                  # Entry point: creates the listening sockets and starts the workers
│   ├── loop.c                  # Per-worker epoll event loop
│   ├── socket.c                # Socket setup, accept clients, read/write, sendfile
│   ├── http.c                  # Parses the request
│   ├── handle_http_request.c   # Router that dispatches requests to handlers
│   ├── handlers_utils.c        # Route handlers
│   ├── response.c              # Preparing HTTP responses
│   ├── list.c                  # Per-worker client list for idle timeouts
│   ├── treatiptable.c          # Sharded IP table / token-bucket rate limiting
│   └── audit.c                 # Audit logs
├── include/                    # Headers
├── www/                        # Static files
└── test/
    ├── loadtest.sh             # Multi-scenario load test (reports saved to test/benchmarks/)
    └── unit_tests/             # Unit tests
```

## Routes

| Method | Path      | Handler Function      |
|--------|-----------|-----------------------|
| GET    | `/status` | `handle_status_get`   |
| POST   | `/data`   | `handle_data_post`    |
| GET    | `*`       | `handle_static_file`  |

## Compilation & Build

> **Note:** Requires a Linux environment (uses Linux-exclusive `epoll` and `sendfile`). Will not compile natively on macOS or Windows.

This project uses a standard `Makefile` to automate the build pipeline. Make sure you have `gcc` and `make` installed on your system.

To compile and build the server executable:
```bash
make
```

## How to run
```bash
./httpserver
```

You can also test it directly in your browser by navigating to http://localhost:8080/ to view the served HTML file.

## How to Test

### Unit tests

The unit tests are built with AddressSanitizer and UndefinedBehaviorSanitizer:
```bash
make -C test/unit_tests
```

### Load test

`test/loadtest.sh` simulates realistic traffic across several scenarios and, on Linux (locally or over SSH), also samples the server's CPU, memory and open file descriptors from `/proc`. Against a remote server, build it with `make NO_RATE_LIMIT=1`. It requires `wrk`, `curl` and `python3`:

```bash
sudo apt update && sudo apt install wrk -y
```


| Scenario    | What it simulates                                                        |
|-------------|--------------------------------------------------------------------------|
| `keepalive` | Clients reusing a persistent connection, like a browser                  |
| `status`    | Same as `keepalive` on `/status` (~60 bytes of JSON, no disk I/O)        |
| `newconn`   | A new TCP connection per request (cost of `accept` and `close`)          |
| `mixed`     | 70% `/`, 20% `/status`, 5% `/cv.pdf` (111 KB), 5% `POST /data`           |
| `bigfile`   | Downloads of `/cv.pdf` (bandwidth / `sendfile`)                          |
| `idle`      | Thousands of connected, almost idle clients alongside 200 active clients |

```bash
./test/loadtest.sh                                  # local server
HOST=1.2.3.4 SSH=root@1.2.3.4 ./test/loadtest.sh    # remote server, metrics over SSH
QUICK=1 ./test/loadtest.sh                          # short run (~1 min)
```

Each run writes a full report and a CSV to `test/benchmarks/`. `HOST`, `PORT`, `DURATION`, `LEVELS` and `SCENARIOS` can be set as environment variables, e.g. `SCENARIOS="keepalive status" ./test/loadtest.sh`.

### Results

Measured on a 4 vCPU x86_64 VPS (Ubuntu 24.04, kernel 6.8) with 4 workers, 15 s per test, with `wrk` running on the same machine over loopback:

```
Test        Clients       Req/s       p50       p99   Transf/s     CPU     RSS
keepalive        50   149975.58  199.00us    2.75ms   191.51MB     2.2     2MB
keepalive       200   169266.57    0.91ms    3.94ms   216.15MB     2.2     4MB
keepalive      1000   125654.85    7.27ms   15.41ms   160.46MB     2.3    14MB
keepalive      4000   105699.78   33.61ms   77.72ms   134.98MB     2.4    53MB
status           50   220967.35  124.00us    2.65ms    32.66MB     1.9     2MB
status          200   254494.06  493.00us    4.26ms    37.86MB     2.0     5MB
status         1000   247671.61    3.30ms    9.55ms    37.08MB     2.0    17MB
status         4000   181527.34   17.80ms   41.09ms    27.18MB     2.1    65MB
newconn          50    42752.36  738.00us    3.11ms    54.39MB     1.8     3MB
newconn         200    49558.34    2.37ms    9.58ms    63.05MB     1.8     5MB
newconn        1000    45642.91    7.01ms   52.02ms    58.07MB     1.8    17MB
mixed            50   128154.17  245.00us    2.68ms   800.78MB     2.2     2MB
mixed           200   139402.25    1.16ms    4.61ms   869.02MB     2.2     5MB
mixed          1000   113706.16    7.97ms   17.20ms   710.78MB     2.3    17MB
mixed          4000   105718.66   33.42ms   73.73ms   659.57MB     2.3    65MB
bigfile          50    28658.60    1.51ms    4.54ms     2.97GB     2.4     2MB
idle+200        200   154835.08    1.02ms    4.47ms   197.72MB     2.2    36MB
```

- **Zero socket errors and zero non-2xx responses** across all 17 runs.
- **~255k req/s** peak on `/status`, which isolates the event loop and HTTP handling, and still **~180k req/s with 4,000 concurrent clients** (p99 41 ms).
- **~170k req/s** peak serving a 1.2 KB HTML file with keep-alive, and still **over 100k req/s with 4,000 concurrent clients** (p99 under 80 ms).
- **~3 GB/s** when serving a 111 KB file with `sendfile()`.
- **2,000 idle connections** held open alongside 200 active clients with none dropped, at ~155k req/s (vs ~169k without them) and 36 MB RSS.
- `CPU` is the number of busy server cores (out of 4) and `RSS` the server's memory halfway through each test. `wrk` used the remaining CPU on the same machine, so the server alone would do better on dedicated hardware.
