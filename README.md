# HttpServer

A simple HTTP server written in C, with basic functionality to process HTTP requests.  
This project was developed as a learning exercise and to demonstrate my knowledge.

## Features

- Handle HTTP `GET` and `POST` requests
- Route requests based on HTTP method and path
- Serve a root route `/` and additional routes like `/status` and `/data`
- Respond with `400 Bad Request` if the request is invalid
- Respond with `404 Not Found` if no route matches
- Modular design: easy to add new routes and handlers

## Project structure
```
httpserver/
│
├── main.c                  # Entry point for the server  
├── response.c              # Preparing HTTP responses  
├── handle_http_request.c   # Router that dispatches requests to handlers  
├── handlers_utils.c        # Utility functions for handlers  
├── loop.c                  # Main server loop using kevent and threads
├── http.c                  # Parses the request 
├── handleIpTable.c         # IP table management / client access control
└── socket.c                # Socket setup, accept clients, read/write
```

## Routes

| Method | Path     | Handler Function       |
|--------|----------|----------------------|
| GET    | `/`      | `handle_root_get`     |
| GET    | `/status`| `handle_status_get`   |
| POST   | `/data`  | `handle_data_post`    |

## Compilation / Build

This projects uses a simple MakeFile. Make sure you have 'gcc' installed

To build the server:

```bash
make
```

## How to run
```bash
./httpserver
```

## How to test
```bash
cd test
./benchmark.sh
```
That shell script will give soomething like this output: 

```
duartesantos@MacBook-Air-de-Duarte test % ./benchmark.sh
Benchmark /
Running 30s test @ http://localhost:8080/
  8 threads and 400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     6.79ms   10.74ms 232.72ms   98.54%
    Req/Sec     1.83k     1.05k    3.34k    65.80%
  168981 requests in 30.09s, 14.50MB read
  Socket errors: connect 392, read 32, write 0, timeout 0
Requests/sec:   5616.16
Transfer/sec:    493.61KB

Benchmark /status
Running 20s test @ http://localhost:8080/status
  1 threads and 10 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   414.59us    5.62ms 217.85ms   99.39%
    Req/Sec    13.46k     4.65k   22.11k    67.86%
  265095 requests in 20.05s, 41.86MB read
Requests/sec:  13223.21
Transfer/sec:      2.09MB
```


