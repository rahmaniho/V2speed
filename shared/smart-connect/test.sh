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
"$TMP_DIR/v2speed_csv_rank" "$ROOT/tests/fixtures/demo_observations.csv" 60000 30000 > "$TMP_DIR/ranking.csv" 2> "$TMP_DIR/ranking.stderr"
grep -q '^rank,server_id,score,' "$TMP_DIR/ranking.csv"
grep -q '^1,beta,' "$TMP_DIR/ranking.csv"
grep -q 'no probes or network checks were run' "$TMP_DIR/ranking.stderr"
"$TMP_DIR/test_smart_connect"
"$TMP_DIR/test_connection_state"
"$TMP_DIR/test_fallback_planner"
"$TMP_DIR/test_smart_connect_session"
"$TMP_DIR/test_smart_connect_measurements"
"$TMP_DIR/test_smart_connect_probe_plan"
