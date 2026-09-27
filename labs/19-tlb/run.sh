#!/usr/bin/env bash
# run.sh —— 页数从 1 扫到 24576：1, 2, 3, 4, 6, 8, 12, 16, ...（每次 ×2，中间插一个 ×1.5 的点）
# 每个点打印"页数 最小值 中位数"（单位 ns/次访问）
# 用法：./run.sh [额外参数，原样传给 tlb，例如 -p、-s、-n、-k 9]
#       TOTAL=32000000 ./run.sh   # 每个点的总访问次数（默认约 1600 万）
set -e
cd "$(dirname "$0")"
make -s tlb
TOTAL=${TOTAL:-16000000}
printf "%8s %10s %10s\n" "页数" "最小(ns)" "中位数(ns)"
for ((p = 1; p <= 16384; p *= 2)); do
  points="$p"
  [ $p -ge 2 ] && points="$p $(( p * 3 / 2 ))"
  for n in $points; do
    trials=$(( TOTAL / n )); [ $trials -lt 1 ] && trials=1
    ./tlb $n $trials -q "$@" | awk '{printf "%8d %10s %10s\n", $1, $2, $3}'
  done
done
