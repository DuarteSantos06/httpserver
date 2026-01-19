#!/bin/bash

# Porta do servidor
PORT=8080

# Duração dos testes
DURATION_SHORT=10s
DURATION_LONG=20s

# Número de threads e conexões
THREADS_SHORT=8
CONNECTIONS_SHORT=200
THREADS_LONG=1
CONNECTIONS_LONG=10

echo "Benchmark /"
wrk -t${THREADS_SHORT} -c${CONNECTIONS_SHORT} -d${DURATION_SHORT} http://localhost:$PORT/

echo ""
echo "Benchmark /status"
wrk -t${THREADS_LONG} -c${CONNECTIONS_LONG} -d${DURATION_LONG} http://localhost:$PORT/status
