#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CC=${CC:-cc}
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/v2speed-smart-connect.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

# CFLAGS is intentionally word-split as additional compiler/linker flags.
COMMON_FLAGS="-std=c11 -Wall -Wextra -Werror -pedantic ${CFLAGS:-}"
"$CC" $COMMON_FLAGS -I "$ROOT/include" \
  "$ROOT/src/smart_connect.c" "$ROOT/tests/test_smart_connect.c" \
  -lm -o "$TMP_DIR/test_smart_connect"
"$CC" $COMMON_FLAGS -I "$ROOT/include" \
  "$ROOT/src/connection_state.c" "$ROOT/tests/test_connection_state.c" \
  -o "$TMP_DIR/test_connection_state"
"$CC" $COMMON_FLAGS -I "$ROOT/include" \
  "$ROOT/src/fallback_planner.c" "$ROOT/tests/test_fallback_planner.c" \
  -lm -o "$TMP_DIR/test_fallback_planner"
"$CC" $COMMON_FLAGS -I "$ROOT/include" \
  "$ROOT/src/smart_connect.c" "$ROOT/src/connection_state.c" \
  "$ROOT/src/fallback_planner.c" "$ROOT/src/smart_connect_session.c" \
  "$ROOT/src/smart_connect_measurements.c" "$ROOT/src/smart_connect_probe_plan.c" \
  "$ROOT/tests/test_smart_connect_session.c" -lm -o "$TMP_DIR/test_smart_connect_session"
"$CC" $COMMON_FLAGS -I "$ROOT/include" \
  "$ROOT/src/smart_connect_measurements.c" "$ROOT/src/smart_connect.c" \
  "$ROOT/src/smart_connect_probe_plan.c" \
  "$ROOT/tests/test_smart_connect_measurements.c" -lm -o "$TMP_DIR/test_smart_connect_measurements"
"$CC" $COMMON_FLAGS -I "$ROOT/include" \
  "$ROOT/src/smart_connect_probe_plan.c" "$ROOT/tests/test_smart_connect_probe_plan.c" \
  -o "$TMP_DIR/test_smart_connect_probe_plan"
"$CC" $COMMON_FLAGS -I "$ROOT/include" \
  "$ROOT/src/smart_connect.c" "$ROOT/src/smart_connect_measurements.c" \
  "$ROOT/src/smart_connect_probe_plan.c" "$ROOT/tools/v2speed_csv_rank.c" \
  -lm -o "$TMP_DIR/v2speed_csv_rank"
"$TMP_DIR/v2speed_csv_rank" "$ROOT/tests/fixtures/demo_observations.csv" 2000 60000 30000 > "$TMP_DIR/ranking.csv" 2> "$TMP_DIR/ranking.stderr"
grep -q '^rank,server_id,score,' "$TMP_DIR/ranking.csv"
grep -q '^1,beta,' "$TMP_DIR/ranking.csv"
grep -q 'no probes or network checks were run' "$TMP_DIR/ranking.stderr"
if "$TMP_DIR/v2speed_csv_rank" "$ROOT/tests/fixtures/demo_observations.csv" 1999 60000 30000 > /dev/null 2>&1; then
  echo 'CSV ranker accepted a future-dated observation.' >&2
  exit 1
fi
cat > "$TMP_DIR/malformed.csv" <<'CSV'
server_id,timestamp_ms,success,measured_mask,throughput_mbps,latency_ms
bad,1000,0,1,0,0
CSV
if "$TMP_DIR/v2speed_csv_rank" "$TMP_DIR/malformed.csv" 1000 60000 30000 > /dev/null 2>&1; then
  echo 'CSV ranker accepted metrics on a failed attempt.' >&2
  exit 1
fi
cat > "$TMP_DIR/stale.csv" <<'CSV'
server_id,timestamp_ms,success,measured_mask,throughput_mbps,latency_ms
old-node,100,1,3,40,20
CSV
"$TMP_DIR/v2speed_csv_rank" "$TMP_DIR/stale.csv" 1000 100 50 > "$TMP_DIR/stale.out" 2> /dev/null
grep -q '^No eligible server has fresh measured observations\.' "$TMP_DIR/stale.out"
printf '%s\n' 'server_id,timestamp_ms,success,measured_mask,throughput_mbps,latency_ms' > "$TMP_DIR/oversized.csv"
i=0
while [ "$i" -lt 65 ]; do
  printf 'bounded,%s,1,1,25,0\n' "$((1000 + i))" >> "$TMP_DIR/oversized.csv"
  i=$((i + 1))
done
if "$TMP_DIR/v2speed_csv_rank" "$TMP_DIR/oversized.csv" 1064 60000 30000 > /dev/null 2>&1; then
  echo 'CSV ranker accepted more than 64 rows for one server.' >&2
  exit 1
fi
"$TMP_DIR/test_smart_connect"
"$TMP_DIR/test_connection_state"
"$TMP_DIR/test_fallback_planner"
"$TMP_DIR/test_smart_connect_session"
"$TMP_DIR/test_smart_connect_measurements"
"$TMP_DIR/test_smart_connect_probe_plan"
