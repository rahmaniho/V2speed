#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CC=${CC:-cc}
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/v2speed-smart-connect.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

COMMON_FLAGS="-std=c11 -Wall -Wextra -Werror -pedantic"
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
  "$ROOT/tests/test_smart_connect_session.c" -lm -o "$TMP_DIR/test_smart_connect_session"
"$CC" $COMMON_FLAGS -I "$ROOT/include" \
  "$ROOT/src/smart_connect_measurements.c" "$ROOT/src/smart_connect.c" \
  "$ROOT/tests/test_smart_connect_measurements.c" -lm -o "$TMP_DIR/test_smart_connect_measurements"
"$CC" $COMMON_FLAGS -I "$ROOT/include" \
  "$ROOT/src/smart_connect_probe_plan.c" "$ROOT/tests/test_smart_connect_probe_plan.c" \
  -o "$TMP_DIR/test_smart_connect_probe_plan"
"$TMP_DIR/test_smart_connect"
"$TMP_DIR/test_connection_state"
"$TMP_DIR/test_fallback_planner"
"$TMP_DIR/test_smart_connect_session"
"$TMP_DIR/test_smart_connect_measurements"
"$TMP_DIR/test_smart_connect_probe_plan"
