#!/bin/bash

URL="http://localhost:8080/"
TOTAL_REQUESTS=100000
CONCURRENCY=200        # concorrência realista
REQUESTS_PER_WORKER=500

echo "Total requests: $TOTAL_REQUESTS"
echo "Concurrency: $CONCURRENCY"
echo "Requests/worker: $REQUESTS_PER_WORKER"
echo ""

start=$(date +%s.%N)

worker() {
  for ((i=0; i<REQUESTS_PER_WORKER; i++)); do
    curl -s --keepalive-time 60 "$URL" > /dev/null
  done
}

export -f worker
export URL REQUESTS_PER_WORKER

seq 1 $CONCURRENCY | xargs -n1 -P$CONCURRENCY bash -c 'worker'

end=$(date +%s.%N)
elapsed=$(echo "$end - $start" | bc)

rps=$(echo "$TOTAL_REQUESTS / $elapsed" | bc)

echo ""
echo "Done"
echo "Time: $elapsed s"
echo "Approx RPS: $rps"
