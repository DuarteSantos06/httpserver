#!/bin/bash

CLIENTS=10000
CONCURRENT=10000

start=$(date +%s.%N)  # segundos com fração

echo "Launching $CLIENTS concurrent clients..."

send_clients() {
    local start_index=$1
    local end_index=$2
    for ((i=start_index; i<=end_index; i++)); do
        printf "Hello from client %d\n" "$i" | nc localhost 8080 &
    done
    wait
}

for ((i=1; i<=CLIENTS; i+=CONCURRENT)); do
    end=$((i+CONCURRENT-1))
    if (( end > CLIENTS )); then end=$CLIENTS; fi
    send_clients $i $end
done

end=$(date +%s.%N)
elapsed=$(echo "$end - $start" | bc)

echo "All clients finished"
echo "Total time: $elapsed seconds"
