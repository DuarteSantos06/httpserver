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
- **Rate limiting.** A 256-shard hash table (one mutex per shard) stores a token bucket per IP, with a background thread removing stale entries. Localhost is exempt so benchmarks are not throttled.


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
    ├── benchmark.sh            # wrk benchmark
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

### Benchmarks

The benchmarking script requires `wrk` installed on the system.

**Install wrk (Ubuntu/Debian):**
```bash
sudo apt update && sudo apt install wrk -y
```
**Run the benchmarks:**
```bash
cd test
./benchmark.sh
```

The script runs `/` and `/status` with 10, 100 and 400 connections, reports p50/p99 latency and errors for each run, and saves the full output to `test/results/`. `HOST`, `PORT`, `DURATION` and `WARMUP` can be set as environment variables, e.g. `DURATION=30s ./benchmark.sh`.

### Results

Measured with 4 workers on an 8-core ARM64 Linux VM (Apple M3), with `wrk` running on the same machine over loopback:

```
Endpoint  Conns        Req/s        p50        p99  Errors
/            10     32232.13   140.00us    11.68ms  none
/           100     36834.72     2.08ms    21.33ms  none
/           400     33112.19    10.81ms    45.02ms  none
/status      10    877446.74     9.00us    42.00us  none
/status     100    988056.40    67.00us     1.44ms  none
/status     400   1049979.90   248.00us     2.96ms  none
```

Memory footprint (RSS): **1.5 MB** idle and **under 10 MB** peak during the full benchmark.

Notes on these numbers:
- `/status` returns about 60 bytes of JSON, so it measures the event loop and HTTP handling rather than I/O. `/` serves a 1.2 KB HTML file from disk.
- `wrk` and the server share the same machine, so they compete for the same CPU cores. Results on separate machines would differ.
