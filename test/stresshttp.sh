#!/bin/bash

URL="http://localhost:8080/"
TOTAL_REQUESTS=10000000   #total requests we want to send
CONCURRENCY=1000       # number of concurrent workers (threads) to send requests
REQUESTS_PER_WORKER=$((TOTAL_REQUESTS / CONCURRENCY))

echo "Total requests: $TOTAL_REQUESTS"
echo "Concurrency: $CONCURRENCY"
echo "Requests per worker: $REQUESTS_PER_WORKER"
echo ""

start=$(date +%s.%N)

worker() {
  for ((i=0; i<REQUESTS_PER_WORKER; i++)); do
    curl -s -o /dev/null "$URL"
  done
}

# Lança os workers em background
for ((w=0; w<CONCURRENCY; w++)); do
  worker &
done

# Espera todos terminarem
wait

end=$(date +%s.%N)
elapsed=$(echo "$end - $start" | bc -l)
rps=$(echo "$TOTAL_REQUESTS / $elapsed" | bc -l)

echo ""
echo "Done"
echo "Total time: $elapsed s"
echo "Approx RPS: $rps"
