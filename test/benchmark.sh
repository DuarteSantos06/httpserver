#!/bin/bash
#
# Benchmark do servidor HTTP com wrk.
# Corre cada endpoint com vários níveis de concorrência e reporta
# req/s, latência p50/p99 e erros, para resultados reproduzíveis.
#
# Uso: ./benchmark.sh            (HOST/PORT/DURATION podem ser definidos por env)

HOST=${HOST:-localhost}
PORT=${PORT:-8080}
DURATION=${DURATION:-20s}
WARMUP=${WARMUP:-5s}

ENDPOINTS=("/" "/status")
CONNECTIONS=(10 100 400)

RESULTS_DIR="$(dirname "$0")/results"
RESULTS_FILE="$RESULTS_DIR/bench_$(date +%Y%m%d_%H%M%S).txt"

# Número de cores do cliente (Linux ou macOS)
if command -v nproc >/dev/null 2>&1; then
    CORES=$(nproc)
else
    CORES=$(sysctl -n hw.ncpu)
fi

if ! command -v wrk >/dev/null 2>&1; then
    echo "Erro: wrk não está instalado." >&2
    exit 1
fi

if ! curl -s -o /dev/null "http://$HOST:$PORT/status"; then
    echo "Erro: servidor não responde em http://$HOST:$PORT" >&2
    exit 1
fi

mkdir -p "$RESULTS_DIR"

{
    echo "=== Ambiente ==="
    echo "Data:      $(date)"
    echo "Cliente:   $(uname -srm), $CORES cores"
    echo "Alvo:      http://$HOST:$PORT"
    echo "Duração:   $DURATION por teste (warmup $WARMUP)"
    echo ""
} | tee "$RESULTS_FILE"

# Aquecimento (resultados descartados)
wrk -t2 -c50 -d"$WARMUP" "http://$HOST:$PORT/" >/dev/null

SUMMARY=()

for endpoint in "${ENDPOINTS[@]}"; do
    for conns in "${CONNECTIONS[@]}"; do
        # wrk exige threads <= conexões
        threads=$(( CORES < conns ? CORES : conns ))

        echo "=== $endpoint | $threads threads | $conns conexões ===" | tee -a "$RESULTS_FILE"
        out=$(wrk -t"$threads" -c"$conns" -d"$DURATION" --latency "http://$HOST:$PORT$endpoint")
        echo "$out" | tee -a "$RESULTS_FILE"
        echo "" | tee -a "$RESULTS_FILE"

        rps=$(echo "$out" | awk '/Requests\/sec/ {print $2}')
        p50=$(echo "$out" | awk '$1 == "50%" {print $2}')
        p99=$(echo "$out" | awk '$1 == "99%" {print $2}')
        errors=$(echo "$out" | awk '/Socket errors/ {gsub(",", ""); print "connect=" $4 " read=" $6 " write=" $8 " timeout=" $10}')
        non2xx=$(echo "$out" | awk '/Non-2xx/ {print $NF}')

        SUMMARY+=("$(printf "%-8s %6s %12s %10s %10s  %s %s" \
            "$endpoint" "$conns" "$rps" "$p50" "$p99" \
            "${errors:-sem erros}" "${non2xx:+non2xx=$non2xx}")")
    done
done

{
    echo "=== Resumo ==="
    printf "%-8s %6s %12s %10s %10s  %s\n" "Endpoint" "Conns" "Req/s" "p50" "p99" "Erros"
    for line in "${SUMMARY[@]}"; do
        echo "$line"
    done
    echo ""
    echo "Resultados guardados em $RESULTS_FILE"
} | tee -a "$RESULTS_FILE"
