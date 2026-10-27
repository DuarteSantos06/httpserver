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
    ├── latency.sh              # Fixed-rate latency test with wrk2
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

Server: 4 vCPU x86_64 VPS (Ubuntu 24.04, kernel 6.8) with 4 workers. Client: a second 4 vCPU VPS in the same datacenter (RTT 0.4 ms, ~5 Gbit/s measured with `iperf3`), 15 s per test:

```
Test        Clients       Req/s       p50       p99   Transf/s     CPU     RSS
keepalive        50   112832.66  377.00us    0.89ms   144.08MB     3.8     2MB
keepalive       200   129764.79    1.41ms    3.21ms   165.71MB     3.9     4MB
keepalive      1000   116772.08    8.03ms   15.89ms   149.11MB     4.0    14MB
keepalive      4000   112471.17   32.45ms   61.30ms   143.62MB     4.0    53MB
status           50   196363.10  217.00us  592.00us    29.03MB     3.1     2MB
status          200   258541.60  664.00us    1.54ms    38.46MB     3.8     5MB
status         1000   212162.30    2.95ms   10.72ms    31.77MB     3.5    17MB
status         4000   159580.10   15.61ms   43.62ms    23.89MB     3.4    64MB
newconn          50    23986.99    1.22ms    5.31ms    30.52MB     2.6     2MB
newconn         200    27577.66    4.86ms   23.42ms    35.08MB     3.1     5MB
newconn        1000    31993.68   22.53ms   81.30ms    40.70MB     3.5    20MB
mixed            50    74844.98  537.00us    2.03ms   467.22MB     2.6     2MB
mixed           200    91195.33    2.03ms    4.68ms   569.44MB     2.6     5MB
mixed          1000    93886.02    8.83ms  141.29ms   585.72MB     3.5    17MB
mixed          4000    80327.10   42.06ms  153.19ms   500.93MB     3.9    65MB
bigfile          50     8308.98    5.26ms   15.84ms     0.86GB     0.7     2MB
idle+200        200   124110.25    1.44ms    3.69ms   158.49MB     3.9    36MB
```

- **Zero socket errors and zero non-2xx responses** across all 17 runs.
- **~259k req/s** peak on `/status`, which isolates the event loop and HTTP handling, and still **~160k req/s with 4,000 concurrent clients** (p99 44 ms).
- **~130k req/s** peak serving a 1.2 KB HTML file with keep-alive, with all 4 cores busy, and still **over 110k req/s with 4,000 concurrent clients** (p99 61 ms).
- **2,000 idle connections** held open alongside 200 active clients with none dropped, at ~124k req/s (vs ~130k without them) and 36 MB RSS.
- `bigfile` is bound by the network, not the server: 0.86 GB/s (~7 Gbit/s) with the server using under one core.
- **~30k new TCP connections per second** with 1,000 clients (`newconn`, where every request opens and closes its own connection), confirmed in two later runs (32k req/s each). These rows come from a run of that scenario alone: in the first full run, 1,000 clients dropped once to ~4k req/s with the server almost idle (0.4 cores) and p99 close to 1 s, the typical delay of a retransmitted SYN. It did not happen again, and the server showed no listen queue overflows, so it points to something transient between the two machines rather than the server.
- `CPU` is the number of busy server cores (out of 4) and `RSS` the server's memory halfway through each test. The p99 values come from `wrk` and are optimistic; see the latency test below.

#### Over loopback

The same test with `wrk` on the server machine itself. Without a network card in the path `/` reaches ~170k req/s and `sendfile()` ~3 GB/s, but `wrk` takes CPU from the server (only ~2.2 of its 4 cores were busy), so these numbers are not a clean measure of either:

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


### Latency test

`wrk` is closed-loop: each connection waits for a response before sending the next request, so when the server stalls `wrk` also stops sending and the slow requests are never measured (*coordinated omission*). The p99 values above are therefore optimistic.

`test/latency.sh` uses [`wrk2`](https://github.com/giltene/wrk2) instead, which sends requests at a constant rate whether the responses arrive or not. It first measures the maximum throughput, then runs at several fractions of it; the point where p99 jumps is the real capacity of the server with acceptable latency.

```bash
./test/latency.sh                                 # local server
MAX=143989 PCTS="50 60 70 75" ./test/latency.sh   # reuse a measured max, custom rates
URL=/status CONNS=1000 ./test/latency.sh          # other endpoint / number of connections
```

`wrk2` is not packaged by most distributions; build it with `git clone https://github.com/giltene/wrk2 && make -C wrk2` and copy `wrk2/wrk` to `/usr/local/bin/wrk2`.

Measured from the second VPS in the same datacenter, `GET /` with 200 connections, 30 s per rate (max 131k req/s):

```
% of max     Target      Req/s       p50       p90       p99     p99.9       Max    CPU
     25%      32752      32618    1.11ms    1.79ms    2.24ms   11.18ms   33.98ms    1.1
     50%      65505      64756    1.39ms    2.33ms    3.17ms    6.49ms   48.77ms    2.2
     75%      98258      97851    1.48ms    5.06ms   81.28ms  147.46ms  222.21ms    3.4
     90%     117909     115732   65.98ms  793.09ms     1.37s     1.91s     2.48s    3.8
    100%     131011     118751     1.33s     3.99s     4.86s     5.23s     5.65s    3.8
```

- **p99 around 3 ms up to ~65k req/s**, with zero errors. At ~98k req/s the median is still 1.5 ms, but the slowest 1% already wait ~80 ms.
- From 90% of the max the server is saturated (3.8 of 4 cores busy), requests queue up and p99 rises to seconds.
- The latency grows smoothly with the load, which is what a server limited by its own CPU looks like.

Before having a second machine, the same test was run on the server itself with the server pinned to 2 cores and `wrk2` to the other 2 (`taskset`): p99 stayed under 5 ms up to ~85k req/s on just 2 cores. Over loopback each request is cheaper (no network card, no interrupts), which is why that number is higher per core. Without pinning, `wrk2` was starved of CPU and reported multi-second stalls that came from the load generator, not the server.
