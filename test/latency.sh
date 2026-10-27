#!/bin/bash
#
# HTTP server latency test at a fixed request rate, using wrk2.
#
# wrk is closed-loop: each connection waits for a response before sending the
# next request, so when the server stalls wrk also stops sending and the slow
# requests are never measured (coordinated omission). wrk2 sends requests at a
# constant rate whether the responses arrive or not, and measures latency from
# the moment each request should have been sent.
#
# The test first measures the maximum throughput (or uses MAX), then runs wrk2
# at several fractions of it. The point where p99 jumps is the real capacity
# of the server with acceptable latency.
#
# Usage:
#   ./latency.sh                                    local server
#   MAX=169000 ./latency.sh                         skip the max throughput run
#   HOST=1.2.3.4 SSH=root@1.2.3.4 ./latency.sh      remote server, metrics over SSH
#   taskset -c 1-3 ./latency.sh                     pin the client to some cores
#
# Variables (with their defaults):
#   HOST=localhost PORT=8080 URL=/ CONNS=200 DURATION=30
#   MAX=                     max req/s; measured with wrk2 when empty
#   PCTS="25 50 75 90 100"   rates to test, as a percentage of MAX
#   WRK2=wrk2                wrk2 binary (it builds as "wrk", so copy it as wrk2)
#   SSH=user@host            reads the server's /proc during the tests
#
# Each run writes its report to test/benchmarks/latency_<date>.txt and
# test/benchmarks/latency_<date>.csv.
#
# wrk2 spends its first ~10 seconds calibrating, so keep DURATION at 30 or more.
# Build the server with `make NO_RATE_LIMIT=1`, otherwise it answers 429.

HOST=${HOST:-localhost}
PORT=${PORT:-8080}
URL=${URL:-/}
CONNS=${CONNS:-200}
DURATION=${DURATION:-30}
MAX=${MAX:-}
PCTS=${PCTS:-"25 50 75 90 100"}
WRK2=${WRK2:-wrk2}
SSH=${SSH:-}

BASE="http://$HOST:$PORT"
BENCH_DIR="$(dirname "$0")/benchmarks"
STAMP=$(date +%Y%m%d_%H%M%S)
LOG="$BENCH_DIR/latency_$STAMP.txt"
CSV="$BENCH_DIR/latency_$STAMP.csv"
TMP=$(mktemp -d)
SSH_CTL="$TMP/ssh.sock"

cleanup() {
    [ -n "$SSH" ] && ssh -o ControlPath="$SSH_CTL" -O exit "$SSH" 2>/dev/null
    rm -rf "$TMP"
}
trap cleanup EXIT

log() { echo "$@" | tee -a "$LOG"; }
die() { echo "Error: $*" >&2; exit 1; }

command -v "$WRK2" >/dev/null 2>&1 || die "$WRK2 is not installed (build it from https://github.com/giltene/wrk2 and copy the binary as wrk2)."
command -v curl    >/dev/null 2>&1 || die "curl is not installed."

# Number of client cores (respects taskset on Linux)
if command -v nproc >/dev/null 2>&1; then
    CORES=$(nproc)
else
    CORES=$(sysctl -n hw.ncpu)
fi
THREADS=$(( CORES < CONNS ? CORES : CONNS ))

# Each connection is a file descriptor; macOS defaults to 256
for n in 65535 24576 10240 4096; do
    ulimit -n "$n" 2>/dev/null && break
done

# -------------------------
# Server metrics (/proc, local or over SSH)
# -------------------------

srv() {
    if [ -n "$SSH" ]; then
        ssh -o ControlMaster=auto -o ControlPath="$SSH_CTL" -o ControlPersist=600 "$SSH" "$1"
    else
        bash -c "$1"
    fi
}

# "<CPU ticks> <RSS in kB>" of the httpserver process
SNAP='pid=$(pgrep -xo httpserver) || exit 1
echo "$(awk "{print \$14+\$15}" /proc/$pid/stat) $(awk "/VmRSS/{print \$2}" /proc/$pid/status)"'

METRICS=0
if [ -n "$SSH" ]; then
    echo "Connecting to $SSH to read server metrics..."
fi
if [ -n "$SSH" ] || [ -d /proc/self ]; then
    if srv "$SNAP" >/dev/null 2>&1; then
        METRICS=1
        CLK=$(srv 'getconf CLK_TCK')
    fi
fi

alive() {
    [ "$(curl -s -o /dev/null -m 3 -w '%{http_code}' "$BASE/status")" = "200" ]
}

# -------------------------
# Initial checks
# -------------------------

code=$(curl -s -o /dev/null -m 3 -w '%{http_code}' "$BASE/status")
[ "$code" = "000" ] && die "the server is not responding at $BASE"
[ "$code" = "429" ] && die "the server answers 429: build it with 'make clean && make NO_RATE_LIMIT=1'"

mkdir -p "$BENCH_DIR"
echo "pct,target_req_s,req_s,p50,p90,p99,p999,max,errors,non2xx,cpu_cores,rss_mb" > "$CSV"

log "=== Environment ==="
log "Date:        $(date)"
log "Client:      $(uname -srm), $CORES cores, $THREADS wrk2 threads"
log "Target:      $BASE$URL"
log "Connections: $CONNS"
log "Duration:    ${DURATION}s per rate"
if [ "$METRICS" = 1 ]; then
    log "Metrics:     server /proc$( [ -n "$SSH" ] && echo " via $SSH")"
else
    log "Metrics:     unavailable (server is not local Linux and SSH is not set)"
fi

# -------------------------
# Max throughput
# -------------------------

# wrk2 with a rate far above what the server can do behaves like wrk
log ""
log "Warm-up (10s)..."
"$WRK2" -t"$THREADS" -c"$CONNS" -d10s -R100000000 "$BASE$URL" >/dev/null 2>&1

if [ -z "$MAX" ]; then
    log ""
    log "=== Max throughput (closed-loop, ${DURATION}s) ==="
    out=$("$WRK2" -t"$THREADS" -c"$CONNS" -d"${DURATION}s" -R100000000 "$BASE$URL" 2>&1)
    echo "$out" >> "$LOG"
    MAX=$(awk '/Requests\/sec/ {printf "%d", $2}' <<< "$out")
    [ -z "$MAX" ] || [ "$MAX" = "0" ] && die "could not measure the max throughput, see $LOG"
    log "-> $MAX req/s"
    sleep 2
fi

# -------------------------
# Fixed rate runs
# -------------------------

SUMMARY=()

# run_rate <percentage of MAX>
run_rate() {
    local pct=$1
    local rate=$(( MAX * pct / 100 ))
    log ""
    log "=== $pct% of max | $rate req/s | $CONNS connections ==="

    local before after
    [ "$METRICS" = 1 ] && before=$(srv "$SNAP")

    local out
    out=$("$WRK2" -t"$THREADS" -c"$CONNS" -d"${DURATION}s" -R"$rate" --timeout 5s --latency "$BASE$URL" 2>&1)
    echo "$out" >> "$LOG"

    local rps p50 p90 p99 p999 pmax errs non2xx
    rps=$(awk '/Requests\/sec/ {printf "%d", $2}' <<< "$out")
    p50=$(awk '$1 == "50.000%" {print $2}' <<< "$out")
    p90=$(awk '$1 == "90.000%" {print $2}' <<< "$out")
    p99=$(awk '$1 == "99.000%" {print $2}' <<< "$out")
    p999=$(awk '$1 == "99.900%" {print $2}' <<< "$out")
    pmax=$(awk '$1 == "100.000%" {print $2}' <<< "$out")
    errs=$(awk '/Socket errors/ {gsub(",", ""); print $4 + $6 + $8 + $10}' <<< "$out")
    non2xx=$(awk '/Non-2xx/ {print $NF}' <<< "$out")
    errs=${errs:-0}
    non2xx=${non2xx:-0}

    if [ -z "$rps" ]; then
        log "!! wrk2 failed:"
        log "$out"
        rps=0
    fi

    local cpu="-" rss="-" t0 t1
    if [ "$METRICS" = 1 ]; then
        after=$(srv "$SNAP")
        read -r t0 _ <<< "$before"
        read -r t1 rss <<< "$after"
        cpu=$(awk -v a="$t0" -v b="$t1" -v c="$CLK" -v d="$DURATION" 'BEGIN{printf "%.1f", (b - a) / c / d}')
        rss=$((rss / 1024))
    fi

    local notes=""
    # Below 95% of the target rate the server (or wrk2) could not keep up,
    # so the latencies of this run include the backlog
    [ "$rps" -lt $(( rate * 95 / 100 )) ] && notes="$notes below-target"
    [ "$errs" != "0" ] && notes="$notes errors=$errs"
    [ "$non2xx" != "0" ] && notes="$notes non2xx=$non2xx"

    SUMMARY+=("$(printf "%8s %10s %10s %9s %9s %9s %9s %9s %6s %7s %s" \
        "$pct%" "$rate" "$rps" "${p50:--}" "${p90:--}" "${p99:--}" "${p999:--}" "${pmax:--}" \
        "$cpu" "$( [ "$rss" = "-" ] && echo - || echo "${rss}MB")" "$notes")")
    echo "$pct,$rate,$rps,$p50,$p90,$p99,$p999,$pmax,$errs,$non2xx,$cpu,$rss" >> "$CSV"

    log "-> $rps req/s, p50 $p50, p99 $p99, p99.9 $p999${notes:+ |$notes}"

    if ! alive; then
        log ""
        log "!! The server stopped responding at $pct% of max"
        print_summary
        exit 1
    fi
    sleep 2   # let the connections close before the next run
}

print_summary() {
    log ""
    log "=== Summary ($BASE$URL, $CONNS connections, max $MAX req/s) ==="
    log "$(printf "%8s %10s %10s %9s %9s %9s %9s %9s %6s %7s %s" \
        "% of max" "Target" "Req/s" "p50" "p90" "p99" "p99.9" "Max" "CPU" "RSS" "Notes")"
    for line in "${SUMMARY[@]}"; do
        log "$line"
    done
    log ""
    log "Latencies are corrected for coordinated omission (wrk2)."
    log "below-target: the server or wrk2 could not keep up with that rate."
    log ""
    log "Report: $LOG"
    log "CSV:    $CSV"
}

for pct in $PCTS; do
    run_rate "$pct"
done

print_summary
