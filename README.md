# HttpServer

A simple HTTP server written in C, with basic functionality to process HTTP requests.  
This project was developed as a learning exercise to demonstrate my knowledge of systems programming.

## Features

- Handles HTTP `GET` and `POST` requests
- Serves static files from a root directory (automatically detecting formats like `.html`)
- Routes requests dynamically based on the HTTP method and path
- Provides predefined routes such as `/status` (JSON state) and `/data` (file persistence)
- Responds with `400 Bad Request` if the HTTP syntax or path is invalid
- Responds with `404 Not Found` if no route or file matches the request
- Features a modular design, making it easy to add new routes and custom handlers

## Project structure
```
httpserver/
│
├── main.c                  # Entry point for the server  
├── response.c              # Preparing HTTP responses  
├── handle_http_request.c   # Router that dispatches requests to handlers  
├── handlers_utils.c        # Utility functions for handlers  
├── loop.c                  # Main server loop using epoll
├── http.c                  # Parses the request 
├── treatiptable.c          # IP table management / client access control
├── audit.c                 # Audit logs
└── socket.c                # Socket setup, accept clients, read/write

```

## Routes

| Method | Path     | Handler Function      |
|--------|----------|---------------------- |
| GET    | `/status`| `handle_status_get`   |
| POST   | `/data`  | `handle_data_post`    |
| GET    | `/`      | `handle_static_file`     |

## Compilation & Build

> **Note:** Requires a Linux environment (uses Linux-exclusive `epoll`). Will not compile natively on macOS or Windows.

This project uses a standard `Makefile` to automate the build pipeline. Make sure you have `gcc` and `make` installed on your system.

To compile and build the server executable:
```bash
make
```

## How to run
```bash
./httpserver
```

## How to Test 

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

The script will output data similar to this:

``` 
Benchmark /
Running 30s test @ http://localhost:8080/
  8 threads and 400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    23.81ms   35.18ms 493.96ms   90.91%
    Req/Sec     3.34k     1.72k   11.87k    71.19%
  790035 requests in 30.10s, 363.16MB read
Requests/sec:  26243.80
Transfer/sec:     12.06MB

Benchmark /status
Running 20s test @ http://localhost:8080/status
  1 threads and 10 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    81.22us  134.96us   9.65ms   99.39%
    Req/Sec    52.16k     2.94k   58.26k    90.05%
  1042795 requests in 20.10s, 166.08MB read
Requests/sec:  51880.72
Transfer/sec:      8.26MB
```

You can also test it directly in your browser by navigating to http://localhost:8080/ to view the served HTML file


