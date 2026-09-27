#!/usr/bin/env bash
# run_demo.sh —— 启动 echo 服务器，同时发起 3 个客户端，再用 -b 打开阻塞模拟做对照
# 用法: ./run_demo.sh [端口]
set -u
PORT=${1:-9090}
cd "$(dirname "$0")"
run_round() {
    local block=$1
    echo "===== 服务器阻塞模拟 -b $block ms ====="
    ./echo_server -p "$PORT" -b "$block" > server.log 2>&1 &
    local spid=$!
    sleep 0.3
    local cpids=()
    for name in A B C; do
        ./client "$name" -p "$PORT" -n 5 -i 50 &
        cpids+=($!)
    done
    wait "${cpids[@]}"
    kill "$spid" 2>/dev/null
    wait "$spid" 2>/dev/null
    echo "--- 服务器日志前 12 行 ---"
    head -n 12 server.log
    echo
}
run_round 0
run_round 100
rm -f server.log
