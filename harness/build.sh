#!/bin/bash
# harness/build.sh — Vitruvian build script (runs inside the dev container)
#
# Layout expected by the container:
#   /workspace/src/            Vitruvian source (bind-mounted from host, ro)
#   /workspace/ws/buildtools/  buildtools cmake build (named volume)
#   /workspace/ws/generated/   main cmake build (named volume)
#   /workspace/signals/        result output (bind-mounted from host)
#
# BUILDTOOLS_DIR is intentionally kept as the relative path ../buildtools
# so that engine.cmake's ${CMAKE_BINARY_DIR}/${BUILDTOOLS_DIR} resolves
# correctly from within /workspace/ws/generated/.
set -e

SRC=/workspace/src
WS=/workspace/ws
SIGNALS=/workspace/signals
BUILD_TYPE="${VOS_BUILD_TYPE:-Debug}"

BUILDTOOLS_DIR="$WS/buildtools"
GENERATED_DIR="$WS/generated"

LOG="$SIGNALS/build.log"
mkdir -p "$SIGNALS"

log() { printf '%s\n' "$*" | tee -a "$LOG"; }

emit_build_result() {
    status=$1
    duration=$2
    cat > "$SIGNALS/build_result.json" <<EOF
{
  "status": "$status",
  "build_type": "$BUILD_TYPE",
  "duration_s": $duration,
  "log": "$LOG"
}
EOF
}

start_ts=$(date +%s)
: > "$LOG"

# Emit a failure record if we exit early for any reason.
_on_error() {
    duration=$(( $(date +%s) - start_ts ))
    emit_build_result "failed" "$duration"
    log "Build FAILED after ${duration}s."
}
trap '_on_error' INT TERM ERR

log "=== Vitruvian container build ==="
log "Source:     $SRC"
log "Workspace:  $WS"
log "Build type: $BUILD_TYPE"
log ""

# ------------------------------------------------------------------
# Step 1 — buildtools (skipped if already present)
# ------------------------------------------------------------------
if [ ! -f "$BUILDTOOLS_DIR/src/bin/rc/rc" ]; then
    log "--- Building buildtools ---"
    mkdir -p "$BUILDTOOLS_DIR"
    cd "$BUILDTOOLS_DIR"
    cmake -DBUILDTOOLS_MODE=1 "$SRC" -GNinja >> "$LOG" 2>&1
    ninja >> "$LOG" 2>&1
    log "buildtools done."
else
    log "buildtools already built, skipping."
fi

# ------------------------------------------------------------------
# Step 2 — configure main build (skipped if already configured)
# ------------------------------------------------------------------
mkdir -p "$GENERATED_DIR"
cd "$GENERATED_DIR"

if [ ! -f "$GENERATED_DIR/build.ninja" ]; then
    log "--- Configuring main build ---"
    HOST_KERNEL=$(uname -r)
    cmake \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DBUILDTOOLS_DIR=../buildtools \
        -DHOST_KERNEL="$HOST_KERNEL" \
        -DKERNEL_RELEASE="$HOST_KERNEL" \
        -DVITRUVIAN_TARGET_ARCH=x86_64 \
        "$SRC" -GNinja >> "$LOG" 2>&1
    log "configure done."
else
    log "Already configured, skipping cmake."
fi

# ------------------------------------------------------------------
# Step 3 — ninja build
# ------------------------------------------------------------------
log "--- ninja build ---"
cd "$GENERATED_DIR"
ninja >> "$LOG" 2>&1

end_ts=$(date +%s)
duration=$((end_ts - start_ts))
log ""
log "Build finished in ${duration}s."

trap - INT TERM ERR
emit_build_result "success" "$duration"
