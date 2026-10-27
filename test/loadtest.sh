#!/bin/bash
#
# HTTP server load test: simulates many concurrent clients across several
# scenarios and measures req/s, latency, errors and (when possible) the
# server's CPU, memory and file descriptors.
#
# Scenarios:
#   keepalive  clients keep the connection open and send requests back to back (browser)
#   status     same as keepalive but GET /status (~60 bytes of JSON, no disk I/O)
#   newconn    every request uses a new connection (different visitors, tests accept)
#   mixed      mixed traffic: 70% /, 20% /status, 5% /cv.pdf, 5% POST /data
#   bigfile    download of /cv.pdf (bandwidth)
#   idle       thousands of connected but almost idle clients + normal load
#
# Usage:
#   ./loadtest.sh                                   local server
#   HOST=1.2.3.4 SSH=root@1.2.3.4 ./loadtest.sh     remote server, metrics over SSH
#   QUICK=1 ./loadtest.sh                           short version (~1 min)
#
# Variables (with their defaults):
#   HOST=localhost PORT=8080 DURATION=15
#   LEVELS="50 200 1000 4000"     concurrent clients in the keepalive, status and mixed scenarios
#   NEWCONN_LEVELS="50 200 1000"  same for newconn (each request uses a client port)
#   IDLE=2000 IDLE_LOAD=200       idle clients and active clients in the idle scenario
#   SCENARIOS="keepalive status newconn mixed bigfile idle"
#   SSH=user@host                 reads the server's /proc during the tests
#
# Each run writes its report to test/benchmarks/benchmark_<date>.txt and
# test/benchmarks/benchmark_<date>.csv.
#
# Against a remote server, build it with `make NO_RATE_LIMIT=1`: every client
# comes from the same IP and the rate limiter would answer 429.
# The mixed scenario writes to data/data.txt on the server (POST /data).

HOST=${HOST:-localhost}
PORT=${PORT:-8080}
QUICK=${QUICK:-0}
if [ "$QUICK" = "1" ]; then
    DURATION=${DURATION:-5}
    LEVELS=${LEVELS:-"50 400"}
    NEWCONN_LEVELS=${NEWCONN_LEVELS:-"50"}
    IDLE=${IDLE:-500}
else
    DURATION=${DURATION:-15}
    LEVELS=${LEVELS:-"50 200 1000 4000"}
    NEWCONN_LEVELS=${NEWCONN_LEVELS:-"50 200 1000"}
    IDLE=${IDLE:-2000}
fi
IDLE_LOAD=${IDLE_LOAD:-200}
SCENARIOS=${SCENARIOS:-"keepalive status newconn mixed bigfile idle"}
SSH=${SSH:-}

BASE="http://$HOST:$PORT"
BENCH_DIR="$(dirname "$0")/benchmarks"
STAMP=$(date +%Y%m%d_%H%M%S)
LOG="$BENCH_DIR/benchmark_$STAMP.txt"
CSV="$BENCH_DIR/benchmark_$STAMP.csv"
TMP=$(mktemp -d)
SSH_CTL="$TMP/ssh.sock"

cleanup() {
    [ -n "$IDLE_PID" ] && kill "$IDLE_PID" 2>/dev/null
    [ -n "$SSH" ] && ssh -o ControlPath="$SSH_CTL" -O exit "$SSH" 2>/dev/null
    rm -rf "$TMP"
}
trap cleanup EXIT

log() { echo "$@" | tee -a "$LOG"; }
die() { echo "Error: $*" >&2; exit 1; }

command -v wrk  >/dev/null 2>&1 || die "wrk is not installed (brew install wrk / apt install wrk)."
command -v curl >/dev/null 2>&1 || die "curl is not installed."
if ! command -v python3 >/dev/null 2>&1; then
    echo "Warning: python3 not found, skipping the idle scenario." >&2
    SCENARIOS=${SCENARIOS//idle/}
fi

# Number of client cores (Linux or macOS)
if command -v nproc >/dev/null 2>&1; then
    CORES=$(nproc)
else
    CORES=$(sysctl -n hw.ncpu)
fi

# Each simulated client is a file descriptor; macOS defaults to 256
for n in 65535 24576 10240 4096; do
    ulimit -n "$n" 2>/dev/null && break
done
NOFILE=$(ulimit -n)

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

# "<CPU ticks> <RSS in kB> <open fds>" of the httpserver process
SNAP='pid=$(pgrep -xo httpserver) || exit 1
echo "$(awk "{print \$14+\$15}" /proc/$pid/stat) $(awk "/VmRSS/{print \$2}" /proc/$pid/status) $(ls /proc/$pid/fd | wc -l)"'

METRICS=0
if [ -n "$SSH" ]; then
    echo "Connecting to $SSH to read server metrics..."
fi
if [ -n "$SSH" ] || [ -d /proc/self ]; then
    if srv "$SNAP" >/dev/null 2>&1; then
        METRICS=1
        CLK=$(srv 'getconf CLK_TCK')
        SRV_CORES=$(srv 'nproc')
    fi
fi

# -------------------------
# Initial checks
# -------------------------

code=$(curl -s -o /dev/null -m 3 -w '%{http_code}' "$BASE/status")
[ "$code" = "000" ] && die "the server is not responding at $BASE"
[ "$code" = "429" ] && die "the server answers 429: build it with 'make clean && make NO_RATE_LIMIT=1'"

# RTT = TCP handshake time (minimum of 5 samples)
RTT=$(for _ in 1 2 3 4 5; do
    curl -s -o /dev/null -m 3 -w '%{time_connect}\n' "$BASE/status"
done | sort -n | head -n1)
RTT_MS=$(awk -v r="$RTT" 'BEGIN{printf "%.2f", r * 1000}')

mkdir -p "$BENCH_DIR"
echo "test,clients,req_s,p50,p99,transfer_s,errors,non2xx,cpu_cores,rss_mb,fds" > "$CSV"

log "=== Environment ==="
log "Date:       $(date)"
log "Client:     $(uname -srm), $CORES cores, ulimit -n $NOFILE"
log "Target:     $BASE"
log "RTT:        $RTT_MS ms"
if [ "$METRICS" = 1 ]; then
    log "Server:     $SRV_CORES cores, CPU/memory metrics enabled"
else
    log "Server:     no metrics (use SSH=user@host or run on the server itself)"
fi
log "Duration:   ${DURATION}s per test"
log ""
log "With an RTT of $RTT_MS ms, each client makes at most ~1000/$RTT_MS requests/s."
log "If the measured latency is almost all RTT, the bottleneck is the network,"
log "not the server (flagged as 'network' in the notes)."

# -------------------------
# Helper scripts
# -------------------------

cat > "$TMP/mixed.lua" <<'LUA'
local reqs

init = function()
    reqs = {
        wrk.format("GET", "/"),
        wrk.format("GET", "/status"),
        wrk.format("GET", "/cv.pdf"),
        wrk.format("POST", "/data", nil, "loadtest\n"),
    }
end

request = function()
    local r = math.random(100)
    if r <= 70 then return reqs[1]
    elseif r <= 90 then return reqs[2]
    elseif r <= 95 then return reqs[3]
    else return reqs[4] end
end
LUA

# Opens N connections and sends a GET /status on each one every ~5s (below
# the server's TIME_OUT), until the <stop> file exists. Writes the number of
# open connections to the <ready> file and, at the end, "opened dropped".
cat > "$TMP/idle.py" <<'PY'
import os, selectors, socket, sys, time

host, port, n = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
ready, stop = sys.argv[4], sys.argv[5]

family, kind, proto, _, addr = socket.getaddrinfo(host, port, type=socket.SOCK_STREAM)[0]
req = ("GET /status HTTP/1.1\r\nHost: %s\r\n\r\n" % host).encode()
sel = selectors.DefaultSelector()

# Non-blocking connects, so it doesn't take n * RTT
pending = set()
for _ in range(n):
    try:
        s = socket.socket(family, kind, proto)
        s.setblocking(False)
        s.connect_ex(addr)
    except OSError:
        break
    sel.register(s, selectors.EVENT_WRITE)
    pending.add(s)

socks = []
deadline = time.time() + 15
while pending and time.time() < deadline:
    for key, _ in sel.select(timeout=0.5):
        s = key.fileobj
        pending.discard(s)
        if s.getsockopt(socket.SOL_SOCKET, socket.SO_ERROR) == 0:
            sel.modify(s, selectors.EVENT_READ)
            socks.append(s)
        else:
            sel.unregister(s)
            s.close()
for s in pending:
    sel.unregister(s)
    s.close()

opened = len(socks)
with open(ready, "w") as f:
    f.write("%d\n" % opened)

dropped = 0
i = 0
next_send = time.time()
end = time.time() + 600
while socks and not os.path.exists(stop) and time.time() < end:
    # 1/50 of the connections every 0.1s: each one sends a request every ~5s
    if time.time() >= next_send:
        step = max(1, len(socks) // 50)
        for s in socks[i:i + step]:
            try:
                s.send(req)
            except OSError:
                pass
        i = (i + step) % len(socks)
        next_send += 0.1
    for key, _ in sel.select(timeout=0.05):
        s = key.fileobj
        try:
            data = s.recv(65536)
        except (BlockingIOError, InterruptedError):
            continue
        except OSError:
            data = b""
        if not data:
            sel.unregister(s)
            s.close()
            socks.remove(s)
            dropped += 1
            if socks:
                i %= len(socks)

with open(ready, "a") as f:
    f.write("%d %d\n" % (opened, dropped))
PY

# -------------------------
# Running a test
# -------------------------

# Converts "36.03ms", "512.00us", "1.20s" to seconds
to_sec() {
    awk -v v="$1" 'BEGIN {
        if (v ~ /us$/) print v / 1e6
        else if (v ~ /ms$/) print v / 1e3
        else if (v ~ /s$/) print v + 0
        else if (v ~ /m$/) print v * 60
        else print 0
    }'
}

alive() {
    [ "$(curl -s -o /dev/null -m 3 -w '%{http_code}' "$BASE/status")" != "000" ]
}

SUMMARY=()
NET_RTTS=1   # minimum RTTs per request in the current scenario (0 = don't check)

print_summary() {
    log ""
    log "=== Summary ==="
    log "$(printf "%-11s %7s %11s %9s %9s %10s %7s %7s  %s" \
        "Test" "Clients" "Req/s" "p50" "p99" "Transf/s" "CPU" "RSS" "Notes")"
    for line in "${SUMMARY[@]}"; do
        log "$line"
    done
    log ""
    if [ "$METRICS" = 1 ]; then
        log "CPU = busy server cores (max $SRV_CORES); RSS = process memory."
    fi
    log "Report saved to $LOG"
    log "CSV saved to    $CSV"
}

section() {
    log ""
    log "################################################################"
    log "# $1"
    log "# $2"
    log "################################################################"
}

# run_test <name> <clients> <wrk arguments...>
run_test() {
    local name=$1 conns=$2
    shift 2

    if [ "$conns" -gt $((NOFILE - 50)) ]; then
        log ""
        log "--- $name | $conns clients: skipped (ulimit -n = $NOFILE)"
        return
    fi

    local threads=$(( CORES < conns ? CORES : conns ))
    log ""
    log "=== $name | $conns clients | $threads threads ==="

    local before after mid_pid
    rm -f "$TMP/mid"
    if [ "$METRICS" = 1 ]; then
        before=$(srv "$SNAP")
        # Memory and fds halfway through the test, with every connection open
        ( sleep $((DURATION / 2)); srv "$SNAP" > "$TMP/mid" ) &
        mid_pid=$!
    fi

    local out
    out=$(wrk -t"$threads" -c"$conns" -d"${DURATION}s" --timeout 5s --latency "$@" 2>&1)
    [ -n "$mid_pid" ] && wait "$mid_pid"
    echo "$out" >> "$LOG"

    local rps p50 p99 tps errs non2xx
    rps=$(awk '/Requests\/sec/ {print $2}' <<< "$out")
    p50=$(awk '$1 == "50%" {print $2}' <<< "$out")
    p99=$(awk '$1 == "99%" {print $2}' <<< "$out")
    tps=$(awk '/Transfer\/sec/ {print $2}' <<< "$out")
    errs=$(awk '/Socket errors/ {gsub(",", ""); print $4 + $6 + $8 + $10}' <<< "$out")
    non2xx=$(awk '/Non-2xx/ {print $NF}' <<< "$out")
    errs=${errs:-0}
    non2xx=${non2xx:-0}

    if [ -z "$rps" ]; then
        log "!! wrk failed:"
        log "$out"
        rps=0
    fi

    local cpu="-" rss="-" fds="-" t0 t1
    if [ "$METRICS" = 1 ]; then
        after=$(srv "$SNAP")
        read -r t0 _ _ <<< "$before"
        read -r t1 _ _ <<< "$after"
        cpu=$(awk -v a="$t0" -v b="$t1" -v c="$CLK" -v d="$DURATION" 'BEGIN{printf "%.1f", (b - a) / c / d}')
        if [ -s "$TMP/mid" ]; then
            read -r _ rss fds < "$TMP/mid"
            rss=$((rss / 1024))
        fi
    fi

    local notes=""
    [ "$errs" != "0" ] && notes="$notes errors=$errs"
    [ "$non2xx" != "0" ] && notes="$notes non2xx=$non2xx"
    if [ "$NET_RTTS" -gt 0 ] && [ -n "$p50" ]; then
        local share
        share=$(awk -v r="$RTT" -v k="$NET_RTTS" -v p="$(to_sec "$p50")" \
            'BEGIN{ if (p > 0 && k * r / p > 0.7) print "network" }')
        [ -n "$share" ] && notes="$notes $share"
    fi
    [ "$fds" != "-" ] && notes="$notes fds=$fds"

    SUMMARY+=("$(printf "%-11s %7s %11s %9s %9s %10s %7s %7s %s" \
        "$name" "$conns" "$rps" "${p50:--}" "${p99:--}" "${tps:--}" \
        "$cpu" "$( [ "$rss" = "-" ] && echo - || echo "${rss}MB")" "$notes")")
    echo "$name,$conns,$rps,$p50,$p99,$tps,$errs,$non2xx,$cpu,$rss,$fds" >> "$CSV"

    log "-> $rps req/s, p50 $p50, p99 $p99${notes:+ |$notes}"

    if ! alive; then
        log ""
        log "!! The server stopped responding after: $name with $conns clients"
        print_summary
        exit 1
    fi
    sleep 2   # let the connections close before the next test
}

# -------------------------
# Scenarios
# -------------------------

log ""
log "Warm-up (5s)..."
wrk -t2 -c50 -d5s "$BASE/" >/dev/null 2>&1

for scenario in $SCENARIOS; do
    case $scenario in
    keepalive)
        section "keepalive: clients with a persistent connection (GET /)" \
                "Each client sends requests back to back on the same connection, like a browser."
        NET_RTTS=1
        for c in $LEVELS; do run_test keepalive "$c" "$BASE/"; done
        ;;
    status)
        section "status: clients with a persistent connection (GET /status)" \
                "~60 bytes of JSON with no disk I/O: measures the event loop and HTTP handling."
        NET_RTTS=1
        for c in $LEVELS; do run_test status "$c" "$BASE/status"; done
        ;;
    newconn)
        section "newconn: a new connection per request (GET /)" \
                "Simulates different visitors; measures the cost of accept and close."
        NET_RTTS=2
        for c in $NEWCONN_LEVELS; do run_test newconn "$c" -H "Connection: close" "$BASE/"; done
        ;;
    mixed)
        section "mixed: mixed traffic with keep-alive" \
                "70% /, 20% /status, 5% /cv.pdf (111 KB), 5% POST /data."
        NET_RTTS=1
        for c in $LEVELS; do run_test mixed "$c" -s "$TMP/mixed.lua" "$BASE"; done
        ;;
    bigfile)
        section "bigfile: download of /cv.pdf (111 KB)" \
                "Measures bandwidth (Transf/s); over the internet the link is usually the limit."
        NET_RTTS=0
        run_test bigfile 50 "$BASE/cv.pdf"
        ;;
    idle)
        n=$IDLE
        [ "$n" -gt $((NOFILE - 100)) ] && n=$((NOFILE - 100))
        section "idle: $n connected, almost idle clients + $IDLE_LOAD active clients" \
                "Each idle client sends 1 request every ~5s. Compare with keepalive $IDLE_LOAD."
        rm -f "$TMP/ready" "$TMP/stop"
        python3 -I "$TMP/idle.py" "$HOST" "$PORT" "$n" "$TMP/ready" "$TMP/stop" &
        IDLE_PID=$!
        for _ in $(seq 1 40); do
            [ -s "$TMP/ready" ] && break
            sleep 0.5
        done
        opened=$(head -n1 "$TMP/ready" 2>/dev/null)
        log "Idle clients connected: ${opened:-0} of $n"
        NET_RTTS=1
        run_test "idle+$IDLE_LOAD" "$IDLE_LOAD" "$BASE/"
        touch "$TMP/stop"
        wait "$IDLE_PID"
        IDLE_PID=
        read -r opened dropped <<< "$(tail -n1 "$TMP/ready")"
        log "Idle clients: ${opened:-0} opened, ${dropped:-0} dropped during the test"
        if [ "${dropped:-0}" != "0" ]; then
            SUMMARY+=("            !! the server closed $dropped of the $opened idle clients")
        fi
        ;;
    *)
        log "Unknown scenario: $scenario"
        ;;
    esac
done

print_summary
