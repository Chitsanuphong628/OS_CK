#!/bin/bash

set -e

if ! command -v g++ >/dev/null 2>&1; then
	echo "g++ is required. Install it in WSL with: sudo apt update && sudo apt install -y g++"
	exit 1
fi

g++ -std=c++17 -pthread ../server.cpp -o ../server
g++ -std=c++17 -pthread ../client.cpp -o ../client

log_dir="logs/exp1"
mkdir -p "$log_dir"
rm -f "$log_dir"/exp1.log "$log_dir"/client-*.log

echo "========================================"
echo " Experiment 1 - Sequential Baseline"
echo "========================================"
echo "Workers: 1"
echo "Synchronization: OFF"
echo "Random Delay: ON (50-500 ms)"
echo "Target Resource: 10"
echo "========================================"

stdbuf -oL ../server 1 0 > >(tee "$log_dir/exp1.log") 2>&1 &
server_pid=$!
sleep 1

client_pids=()
for client_id in 1 2 3 4 5; do
	{
		printf 'RESERVE 10\nQUIT\n' |
			../client "$client_id" 2>&1 |
			tee "$log_dir/client-${client_id}.log"
	} &
	client_pids+=("$!")
done

for client_pid in "${client_pids[@]}"; do
	wait "$client_pid"
done

kill -INT "$server_pid" 2>/dev/null || true
wait "$server_pid" 2>/dev/null || true

echo "========================================"
echo " Client results"
echo "========================================"
grep -H "Server:" "$log_dir"/client-*.log || true