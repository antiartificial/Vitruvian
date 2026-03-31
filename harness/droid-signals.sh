#!/bin/sh
# harness/droid-signals.sh — aggregate results into a single status.json
#
# Reads partial result files written by build.sh, runsuite-json.sh, and
# vm-run.sh, then produces harness/signals/status.json — the canonical
# signal file that droid workers poll to understand system state.
#
# Can be called on the host after any tier completes.
# Usage:
#   ./harness/droid-signals.sh [--tier 0|1]
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SIGNALS="$REPO_ROOT/harness/signals"
OUT="$SIGNALS/status.json"

TIER="${1:-}"
if [ "$TIER" = "--tier" ]; then
    TIER="$2"
fi
[ -z "$TIER" ] && TIER="0"

# --- helpers ---

read_json_field() {
    file="$1"; field="$2"; default="$3"
    if [ ! -f "$file" ]; then
        echo "$default"
        return
    fi
    if command -v jq > /dev/null 2>&1; then
        val=$(jq -r ".$field // empty" "$file" 2>/dev/null)
        echo "${val:-$default}"
    else
        # Minimal grep/sed fallback for simple string and integer scalars.
        val=$(grep -o "\"${field}\": *\"[^\"]*\"" "$file" 2>/dev/null \
              | head -1 | sed 's/.*": *"\(.*\)"/\1/')
        if [ -z "$val" ]; then
            val=$(grep -o "\"${field}\": *[0-9][0-9]*" "$file" 2>/dev/null \
                  | head -1 | sed 's/.*": *\([0-9]*\)/\1/')
        fi
        echo "${val:-$default}"
    fi
}

iso8601() { date -u '+%Y-%m-%dT%H:%M:%SZ'; }

git_commit() {
    git -C "$REPO_ROOT" rev-parse --short HEAD 2>/dev/null || echo "unknown"
}

git_branch() {
    git -C "$REPO_ROOT" rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown"
}

# Escape a value for safe embedding as a JSON string (backslash then double-quote).
_j() { printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'; }

# --- read partial signals ---

BUILD_STATUS=$(read_json_field "$SIGNALS/build_result.json" "status" "unknown")
BUILD_DURATION=$(read_json_field "$SIGNALS/build_result.json" "duration_s" "0")
BUILD_TYPE=$(read_json_field "$SIGNALS/build_result.json" "build_type" "Debug")
BUILD_LOG=$(read_json_field "$SIGNALS/build_result.json" "log" "")

TEST_STATUS=$(read_json_field "$SIGNALS/test_result.json" "status" "skip")
TEST_TOTAL=$(read_json_field "$SIGNALS/test_result.json" "total" "0")
TEST_PASS=$(read_json_field "$SIGNALS/test_result.json" "passed" "0")
TEST_FAIL=$(read_json_field "$SIGNALS/test_result.json" "failed" "0")
TEST_SKIP=$(read_json_field "$SIGNALS/test_result.json" "skipped" "0")
TEST_TIMEOUT=$(read_json_field "$SIGNALS/test_result.json" "timeout" "0")
TEST_DURATION=$(read_json_field "$SIGNALS/test_result.json" "duration_s" "0")
TEST_LOG_DIR=$(read_json_field "$SIGNALS/test_result.json" "log_dir" "")

# Extract individual test results as a raw JSON array
if [ -f "$SIGNALS/test_result.json" ] && command -v jq > /dev/null 2>&1; then
    TEST_RESULTS=$(jq -c '.results // []' "$SIGNALS/test_result.json" 2>/dev/null)
else
    TEST_RESULTS="[]"
fi

VM_STATUS=$(read_json_field "$SIGNALS/vm_status.json" "status" "not_running")
VM_PID=$(read_json_field "$SIGNALS/vm_status.json" "pid" "0")
VM_SSH_PORT=$(read_json_field "$SIGNALS/vm_status.json" "ssh_port" "2222")

# --- overall signal ---

if [ "$BUILD_STATUS" = "success" ] && [ "$TEST_STATUS" = "pass" ]; then
    OVERALL="green"
elif [ "$BUILD_STATUS" = "failed" ]; then
    OVERALL="red"
elif [ "$TEST_STATUS" = "fail" ]; then
    OVERALL="red"
elif [ "$TEST_STATUS" = "skip" ] && [ "$BUILD_STATUS" = "success" ]; then
    OVERALL="yellow"
else
    OVERALL="unknown"
fi

# --- pre-compute JSON-safe string values ---
_GENERATED_AT=$(iso8601)
_COMMIT=$(git_commit)
_BRANCH=$(_j "$(git_branch)")
_SOURCE_PATH=$(_j "$REPO_ROOT")
_BUILD_LOG=$(_j "$BUILD_LOG")
_TEST_LOG_DIR=$(_j "$TEST_LOG_DIR")

# --- write status.json ---

cat > "$OUT" <<EOF
{
  "generated_at": "$_GENERATED_AT",
  "signal": "$OVERALL",
  "commit": "$_COMMIT",
  "branch": "$_BRANCH",
  "source_path": "$_SOURCE_PATH",
  "tier": $TIER,
  "build": {
    "status": "$BUILD_STATUS",
    "type": "$BUILD_TYPE",
    "duration_s": $BUILD_DURATION,
    "log": "$_BUILD_LOG"
  },
  "tests": {
    "status": "$TEST_STATUS",
    "total": $TEST_TOTAL,
    "passed": $TEST_PASS,
    "failed": $TEST_FAIL,
    "skipped": $TEST_SKIP,
    "timeout": $TEST_TIMEOUT,
    "duration_s": $TEST_DURATION,
    "log_dir": "$_TEST_LOG_DIR",
    "results": $TEST_RESULTS
  },
  "vm": {
    "status": "$VM_STATUS",
    "pid": $VM_PID,
    "ssh_host": "127.0.0.1",
    "ssh_port": $VM_SSH_PORT,
    "ssh_user": "root",
    "ssh_pass": "live"
  }
}
EOF

echo "Signal written: $OUT  [$OVERALL]"

# Print a compact summary for the calling script / CI log
echo ""
echo "  build:   $BUILD_STATUS  (${BUILD_DURATION}s)"
echo "  tests:   $TEST_STATUS  total=$TEST_TOTAL pass=$TEST_PASS fail=$TEST_FAIL skip=$TEST_SKIP"
echo "  vm:      $VM_STATUS"
