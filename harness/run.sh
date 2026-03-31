#!/bin/sh
# harness/run.sh — Vitruvian local testing orchestrator (runs on macOS host)
#
# Detects what changed, picks the right testing tier, invokes Docker (Tier 0)
# or QEMU (Tier 1), and emits harness/signals/status.json for droid workers.
#
# Usage:
#   ./harness/run.sh                   # auto-detect tier from git diff
#   ./harness/run.sh --tier 0          # force Docker-only
#   ./harness/run.sh --tier 1          # force VM test after Docker build
#   ./harness/run.sh --build-only      # build without running tests
#   ./harness/run.sh --test-only       # skip build, run tests only
#   ./harness/run.sh --suite system    # system | beapi | both (default: both)
#   ./harness/run.sh --rebuild         # wipe named build volume and rebuild
#   ./harness/run.sh --vm-stop         # stop a running VM
#   ./harness/run.sh --vm-status       # print VM status

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SIGNALS="$REPO_ROOT/harness/signals"

# --- defaults ---
FORCE_TIER=""
BUILD=1
TEST=1
SUITE="both"
REBUILD=0
VM_ONLY_ACTION=""
export VOS_BUILD_TYPE="${VOS_BUILD_TYPE:-Debug}"

# --- argument parsing ---
while [ "$#" -gt 0 ]; do
    case "$1" in
        --tier)       FORCE_TIER="$2";  shift 2 ;;
        --build-only) TEST=0;           shift   ;;
        --test-only)  BUILD=0;          shift   ;;
        --suite)      SUITE="$2";       shift 2 ;;
        --rebuild)    REBUILD=1;        shift   ;;
        --release)    VOS_BUILD_TYPE=Release; shift ;;
        --vm-stop)    VM_ONLY_ACTION=stop;    shift ;;
        --vm-status)  VM_ONLY_ACTION=status;  shift ;;
        *)
            echo "Unknown option: $1"
            echo "Run $0 --help for usage."
            exit 1 ;;
    esac
done

# --- VM-only actions ---
if [ -n "$VM_ONLY_ACTION" ]; then
    "$SCRIPT_DIR/vm-run.sh" "--$VM_ONLY_ACTION"
    exit $?
fi

# --- prerequisite checks ---
check_docker() {
    if ! command -v docker > /dev/null 2>&1; then
        echo "ERROR: docker not found. Install Docker Desktop for Mac."
        exit 1
    fi
    if ! docker info > /dev/null 2>&1; then
        echo "ERROR: Docker daemon not running. Start Docker Desktop."
        exit 1
    fi
}

# --- tier auto-detection ---
# Checks both the working tree AND the most recent commit so that a freshly
# committed kernel change is not silently routed to Tier 0.
detect_tier() {
    combined=$(
        git -C "$REPO_ROOT" diff --name-only HEAD 2>/dev/null
        git -C "$REPO_ROOT" diff --name-only HEAD~1 HEAD 2>/dev/null
    )
    if echo "$combined" | grep -qE '^src/system/kernel/nexus/|^src/system/kernel/'; then
        echo "1"
    else
        echo "0"
    fi
}

# --- build the docker image if needed ---
ensure_image() {
    if ! docker image inspect vitruvian-dev > /dev/null 2>&1; then
        echo "Building Docker image vitruvian-dev..."
        docker compose -f "$REPO_ROOT/docker-compose.yml" build build
    fi
}

# --- main ---

mkdir -p "$SIGNALS"

check_docker

TIER="${FORCE_TIER:-$(detect_tier)}"

echo "============================================"
echo " Vitruvian Harness"
echo "============================================"
echo "  Repo:       $REPO_ROOT"
echo "  Build type: $VOS_BUILD_TYPE"
echo "  Tier:       $TIER"
echo "  Suite:      $SUITE"
echo "  Build:      $BUILD   Test: $TEST"
echo "============================================"
echo ""

ensure_image

# Wipe build volume if --rebuild
if [ "$REBUILD" -eq 1 ]; then
    echo "Wiping build volume vitruvian-build..."
    docker volume rm vitruvian-build 2>/dev/null || true
fi

export VOS_SUITE="$SUITE"

# --- Tier 0: Docker build + test ---

if [ "$BUILD" -eq 1 ]; then
    echo "--- Tier 0: build ---"
    docker compose -f "$REPO_ROOT/docker-compose.yml" run --rm build || {
        "$SCRIPT_DIR/droid-signals.sh" --tier "$TIER"
        exit 1
    }
fi

if [ "$TEST" -eq 1 ]; then
    echo ""
    echo "--- Tier 0: test ---"
    # Run tests; non-zero exit if failures, but we still emit signals
    docker compose -f "$REPO_ROOT/docker-compose.yml" run --rm test || true
fi

"$SCRIPT_DIR/droid-signals.sh" --tier "$TIER"

# --- Tier 1: VM (only if tier == 1 and we have an ISO) ---

if [ "$TIER" = "1" ]; then
    ISO="$REPO_ROOT/generated.x86/image_tree/vitruvian-custom.iso"
    if [ ! -f "$ISO" ]; then
        echo ""
        echo "WARNING: Tier 1 requested but ISO not found at $ISO"
        echo "  Build the ISO first: cd generated.x86 && ../build/scripts/mkiso.sh"
        echo "  Continuing with Tier 0 results only."
    else
        echo ""
        echo "--- Tier 1: VM ---"
        "$SCRIPT_DIR/vm-run.sh"

        VM_SSH_PORT=$(grep '"ssh_port"' "$SIGNALS/vm_status.json" 2>/dev/null \
            | grep -o '[0-9]*' | head -1)
        VM_SSH_PORT="${VM_SSH_PORT:-2222}"

        # vm-run.sh already waited for the port; no extra sleep needed.

        # sshpass lets us supply the password non-interactively.
        # Install: macOS → brew install sshpass  Linux → apt-get install sshpass
        if ! command -v sshpass > /dev/null 2>&1; then
            echo "WARNING: sshpass not found — skipping VM test collection."
            echo "  Install: brew install sshpass  (macOS)"
            echo "           apt-get install sshpass  (Linux)"
        else
            SSH="sshpass -p live ssh -o StrictHostKeyChecking=no -o ConnectTimeout=10 -p $VM_SSH_PORT root@127.0.0.1"
            SCP="sshpass -p live scp -o StrictHostKeyChecking=no -P $VM_SSH_PORT"

            $SCP "$REPO_ROOT/src/tests/runsuite.sh" \
                "root@127.0.0.1:/tmp/runsuite.sh" 2>/dev/null || true

            $SSH "chmod +x /tmp/runsuite.sh && /tmp/runsuite.sh 2>&1" \
                > "$SIGNALS/vm_test_output.log" 2>&1 || true

            echo "VM test output: $SIGNALS/vm_test_output.log"
        fi

        "$SCRIPT_DIR/droid-signals.sh" --tier 1
    fi
fi

# --- final summary ---
echo ""
SIGNAL=$(grep '"signal"' "$SIGNALS/status.json" 2>/dev/null \
    | sed 's/.*"signal": *"\([^"]*\)".*/\1/')
echo "============================================"
echo " Result: ${SIGNAL:-unknown}"
echo " Signals: $SIGNALS/status.json"
echo "============================================"

# Exit non-zero if anything failed
if [ "$SIGNAL" = "red" ]; then
    exit 1
fi
