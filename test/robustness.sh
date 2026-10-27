#!/bin/bash
#
# Testes de robustez do servidor HTTP: pedidos inválidos, condições
# difíceis, timeouts e rate limiting. Depois de cada teste confirma que o
# servidor continua vivo.
#
# Uso: ./robustness.sh             (HOST/PORT podem ser definidos por env)
#      QUICK=1 ./robustness.sh     (salta os testes de timeout, ~30s)
#
# Requer Linux (bash com /dev/tcp, timeout, curl --interface).

HOST=${HOST:-localhost}
PORT=${PORT:-8080}
QUICK=${QUICK:-0}
IDLE_TIMEOUT=15            # TIME_OUT em include/list.h
AUDIT_LOG=${AUDIT_LOG:-"$(dirname "$0")/../audit/audit.log"}
OTHER_IP=127.0.0.2         # origem "não localhost" para o rate limit

BASE="http://$HOST:$PORT"
BASE4="http://127.0.0.1:$PORT"

PASS=0
FAIL=0
SKIP=0

# Envia bytes crus (aceita \r\n) e imprime a resposta. $2 = tempo máximo
raw() {
    timeout "${2:-2}" bash -c 'exec 3<>"/dev/tcp/$0/$1" || exit 1; printf "%b" "$2" >&3; cat <&3' \
        "$HOST" "$PORT" "$1" 2>/dev/null
}

# Código HTTP da primeira linha da resposta
status_of() {
    printf '%s' "$1" | head -n1 | awk '{print $2}'
}

# Qualquer resposta HTTP (até um 429) conta como vivo
alive() {
    [ "$(curl -s -o /dev/null -m 2 -w '%{http_code}' "$BASE/status")" != "000" ]
}

summary() {
    echo ""
    echo "=== Resumo: $PASS passaram, $FAIL falharam, $SKIP saltados ==="
}

# check <nome> <esperado> <obtido>
check() {
    if [ "$2" = "$3" ]; then
        PASS=$((PASS + 1))
        printf "  \033[32m✓\033[0m %s\n" "$1"
    else
        FAIL=$((FAIL + 1))
        printf "  \033[31m✗\033[0m %s (esperado: %s, obtido: %s)\n" "$1" "$2" "${3:-nada}"
    fi
    if ! alive; then
        echo "  !! O servidor deixou de responder depois de: $1"
        summary
        exit 1
    fi
}

skip() {
    SKIP=$((SKIP + 1))
    printf "  - %s (saltado: %s)\n" "$1" "$2"
}

# Segundos até o servidor fechar uma conexão onde se envia $1 e se fica à espera
seconds_until_close() {
    local start end
    start=$(date +%s)
    timeout $((IDLE_TIMEOUT + 10)) bash -c 'exec 3<>"/dev/tcp/$0/$1" || exit 1; printf "%b" "$2" >&3; cat <&3 >/dev/null' \
        "$HOST" "$PORT" "$1" 2>/dev/null
    end=$(date +%s)
    echo $((end - start))
}

in_range() {
    [ "$1" -ge "$2" ] && [ "$1" -le "$3" ] && echo "sim" || echo "não ($1 s)"
}

if ! alive; then
    echo "Erro: servidor não responde em $BASE" >&2
    exit 1
fi

echo "=== Pedidos inválidos ==="

check "lixo -> 400" 400 \
    "$(status_of "$(raw 'LIXO\r\n\r\n')")"

check "linha de pedido incompleta -> 400" 400 \
    "$(status_of "$(raw 'GET /\r\n\r\n')")"

check "header > 4 KB -> 413" 413 \
    "$(curl -s -o /dev/null -m 3 -w '%{http_code}' -H "X-A: $(head -c 5000 /dev/zero | tr '\0' a)" "$BASE/")"

check "ficheiro inexistente -> 404" 404 \
    "$(curl -s -o /dev/null -m 2 -w '%{http_code}' "$BASE/nao-existe")"

check "método sem rota -> 404" 404 \
    "$(status_of "$(raw 'DELETE / HTTP/1.1\r\nConnection: close\r\n\r\n')")"

audit_before=$(wc -l < "$AUDIT_LOG" 2>/dev/null || echo 0)
check "path traversal -> 400" 400 \
    "$(curl -s -o /dev/null -m 2 -w '%{http_code}' --path-as-is "$BASE/../../etc/passwd")"
if [ -f "$AUDIT_LOG" ]; then
    audit_after=$(wc -l < "$AUDIT_LOG")
    check "path traversal registado no audit log" "sim" \
        "$([ "$audit_after" -gt "$audit_before" ] && echo sim || echo não)"
else
    skip "path traversal registado no audit log" "$AUDIT_LOG não existe"
fi

check "POST sem corpo -> 400" 400 \
    "$(status_of "$(raw 'POST /data HTTP/1.1\r\nContent-Length: 0\r\nConnection: close\r\n\r\n')")"

check "Content-Length negativo -> 400" 400 \
    "$(status_of "$(raw 'POST /data HTTP/1.1\r\nContent-Length: -5\r\nConnection: close\r\n\r\n')")"

check "Content-Length enorme -> 400" 400 \
    "$(status_of "$(raw 'POST /data HTTP/1.1\r\nContent-Length: 99999999\r\nConnection: close\r\n\r\n')")"

echo ""
echo "=== Pedidos válidos em condições difíceis ==="

split=$(timeout 4 bash -c 'exec 3<>"/dev/tcp/$0/$1" || exit 1
    printf "GET / HTT" >&3; sleep 1
    printf "P/1.1\r\nHost: x\r\nConnection: close\r\n\r\n" >&3
    cat <&3' "$HOST" "$PORT" 2>/dev/null)
check "pedido enviado aos bocados -> 200" 200 "$(status_of "$split")"

pipelined=$(raw 'GET /status HTTP/1.1\r\n\r\nGET /status HTTP/1.1\r\nConnection: close\r\n\r\n' 3)
check "pipelining: 2 pedidos -> 2 respostas" 2 \
    "$(printf '%s' "$pipelined" | grep -o 'HTTP/1.1 [0-9]\{3\}' | wc -l | tr -d ' ')"

for _ in $(seq 1 20); do
    curl -s -m 2 "$BASE/cv.pdf" | head -c 100 >/dev/null &
done
wait
check "clientes fecham a meio de um ficheiro (20x)" 200 \
    "$(curl -s -o /dev/null -m 2 -w '%{http_code}' "$BASE/status")"

echo ""
echo "=== Timeouts ==="

if [ "$QUICK" = "1" ]; then
    skip "cliente parado é fechado" "QUICK=1"
    skip "corpo prometido e não enviado é fechado" "QUICK=1"
else
    echo "  (cada teste demora ~${IDLE_TIMEOUT}s)"
    check "cliente parado é fechado em ~${IDLE_TIMEOUT}s" "sim" \
        "$(in_range "$(seconds_until_close '')" $((IDLE_TIMEOUT - 1)) $((IDLE_TIMEOUT + 3)))"

    check "corpo prometido e não enviado é fechado em ~${IDLE_TIMEOUT}s" "sim" \
        "$(in_range "$(seconds_until_close 'POST /data HTTP/1.1\r\nContent-Length: 100\r\n\r\nabc')" \
            $((IDLE_TIMEOUT - 1)) $((IDLE_TIMEOUT + 3)))"
fi

echo ""
echo "=== Rate limit (origem $OTHER_IP) ==="

if ! curl -s -o /dev/null -m 2 --interface "$OTHER_IP" "$BASE4/status"; then
    skip "rate limit" "não foi possível ligar a partir de $OTHER_IP"
else
    sleep 2  # deixa os tokens encherem

    codes=$(for _ in $(seq 1 20); do
        curl -s -o /dev/null -m 2 -w '%{http_code}\n' --interface "$OTHER_IP" "$BASE4/status"
    done)
    echo "  20 conexões seguidas: $(echo "$codes" | sort | uniq -c | awk '{printf "%s×%s ", $1, $2}')"
    check "primeira conexão -> 200" 200 "$(echo "$codes" | head -n1)"
    check "rajada acaba por dar 429" "sim" \
        "$(echo "$codes" | grep -q '^429$' && echo sim || echo não)"

    sleep 2
    check "depois de esperar, volta a dar 200" 200 \
        "$(curl -s -o /dev/null -m 2 -w '%{http_code}' --interface "$OTHER_IP" "$BASE4/status")"

    sleep 2
    codes=$(seq 1 200 | xargs -P 50 -I{} \
        curl -s -o /dev/null -m 3 -w '%{http_code}\n' --interface "$OTHER_IP" "$BASE4/status")
    echo "  200 conexões em paralelo: $(echo "$codes" | sort | uniq -c | awk '{printf "%s×%s ", $1, $2}')"
    check "em paralelo só há 200 ou 429" "sim" \
        "$(echo "$codes" | grep -qvE '^(200|429)$' && echo não || echo sim)"

    sleep 2
    urls=$(for _ in $(seq 1 20); do printf '%s ' "$BASE4/status"; done)
    # shellcheck disable=SC2086
    ka=$(curl -s -m 5 --interface "$OTHER_IP" -w '\n%{http_code}\n' $urls | grep -cx 200)
    check "keep-alive: 20 pedidos numa conexão -> 20×200" 20 "$ka"
fi

# Por último, para não afetar os testes anteriores se falhar
codes=$(for _ in $(seq 1 20); do
    curl -s -o /dev/null -m 2 -w '%{http_code}\n' -4 "$BASE4/status"
done)
check "127.0.0.1 (IPv4) está isento: 20 conexões sem 429" 0 \
    "$(echo "$codes" | grep -cx 429)"

summary
[ "$FAIL" -eq 0 ]
