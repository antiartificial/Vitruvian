#!/bin/sh
# harness/vm-run.sh — Tier 1 QEMU VM launcher (macOS + Linux)
#
# On macOS: uses Apple Hypervisor Framework (-accel hvf) via Homebrew qemu
# On Linux: uses KVM (-accel kvm) — equivalent to the existing runkvm.sh
#
# Usage:
#   ./harness/vm-run.sh [--iso PATH] [--ram 4G] [--cpus 2] [--ssh-port 2222]
#   ./harness/vm-run.sh --stop
#   ./harness/vm-run.sh --status
#
# The VM is started in the background. SSH access (root:live) becomes
# available at 127.0.0.1:${SSH_PORT} once the system has booted.
# A PID file is written to harness/signals/vm.pid.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SIGNALS="$REPO_ROOT/harness/signals"
PID_FILE="$SIGNALS/vm.pid"
STATUS_SNIPPET="$SIGNALS/vm_status.json"

ISO="${VOS_ISO:-$REPO_ROOT/generated.x86/image_tree/vitruvian-custom.iso}"
RAM="${VOS_VM_RAM:-4G}"
CPUS="${VOS_VM_CPUS:-2}"
SSH_PORT="${VOS_VM_SSH_PORT:-2222}"

mkdir -p "$SIGNALS"

# --- helpers ---

detect_accel() {
    uname_s=$(uname -s)
    if [ "$uname_s" = "Darwin" ]; then
        # Apple Hypervisor Framework
        echo "hvf"
    elif [ -e /dev/kvm ]; then
        echo "kvm"
    else
        # Software fallback — slow, but functional
        echo "tcg"
    fi
}

detect_qemu() {
    # Homebrew on Apple Silicon puts qemu in /opt/homebrew/bin
    for candidate in \
        qemu-system-x86_64 \
        /opt/homebrew/bin/qemu-system-x86_64 \
        /usr/local/bin/qemu-system-x86_64; do
        if command -v "$candidate" > /dev/null 2>&1 || [ -x "$candidate" ]; then
            echo "$candidate"
            return
        fi
    done
    echo ""
}

write_vm_status() {
    cat > "$STATUS_SNIPPET" <<EOF
{
  "status": "$1",
  "pid": $2,
  "ssh_host": "127.0.0.1",
  "ssh_port": $SSH_PORT,
  "ssh_user": "root",
  "ssh_pass": "live",
  "iso": "$ISO"
}
EOF
}

# --- subcommands ---

cmd_stop() {
    if [ -f "$PID_FILE" ]; then
        pid=$(cat "$PID_FILE")
        if kill -0 "$pid" 2>/dev/null; then
            kill "$pid"
            echo "VM stopped (PID $pid)."
        else
            echo "VM not running (stale PID $pid)."
        fi
        rm -f "$PID_FILE"
    else
        echo "No VM PID file found."
    fi
    write_vm_status "not_running" 0
}

cmd_status() {
    if [ -f "$PID_FILE" ]; then
        pid=$(cat "$PID_FILE")
        if kill -0 "$pid" 2>/dev/null; then
            echo "VM running (PID $pid), SSH: ssh -p $SSH_PORT root@127.0.0.1"
            write_vm_status "running" "$pid"
        else
            echo "VM not running (stale PID file)."
            write_vm_status "not_running" 0
        fi
    else
        echo "VM not started."
        write_vm_status "not_running" 0
    fi
}

cmd_start() {
    # Guard against a second invocation when a VM is already running.
    if [ -f "$PID_FILE" ]; then
        pid=$(cat "$PID_FILE")
        if kill -0 "$pid" 2>/dev/null; then
            echo "VM already running (PID $pid). Use --stop first."
            exit 1
        fi
        rm -f "$PID_FILE"
    fi

    QEMU=$(detect_qemu)
    if [ -z "$QEMU" ]; then
        echo "ERROR: qemu-system-x86_64 not found."
        echo "  macOS:  brew install qemu"
        echo "  Linux:  apt-get install qemu-system-x86"
        exit 1
    fi

    if [ ! -f "$ISO" ]; then
        echo "ERROR: ISO not found at $ISO"
        echo "  Build the ISO first:  cd generated.x86 && ../build/scripts/mkiso.sh"
        echo "  Or set VOS_ISO=/path/to/your.iso"
        exit 1
    fi

    ACCEL=$(detect_accel)
    echo "Starting VM:"
    echo "  QEMU:     $QEMU"
    echo "  Accel:    $ACCEL"
    echo "  ISO:      $ISO"
    echo "  RAM:      $RAM"
    echo "  CPUs:     $CPUS"
    echo "  SSH port: $SSH_PORT (127.0.0.1:$SSH_PORT → VM:22)"
    echo ""

    ACCEL_FLAGS="-accel $ACCEL"
    # hvf does not need -cpu host; kvm does for best performance
    if [ "$ACCEL" = "kvm" ]; then
        CPU_FLAG="-cpu host"
    else
        CPU_FLAG=""
    fi

    # Log VM output to file
    VM_LOG="$SIGNALS/vm.log"

    # shellcheck disable=SC2086
    "$QEMU" \
        -cdrom "$ISO" -boot d \
        -m "$RAM" \
        $CPU_FLAG \
        -smp "sockets=1,cores=$CPUS,threads=1" \
        $ACCEL_FLAGS \
        -netdev "user,id=net0,hostfwd=tcp::${SSH_PORT}-:22" \
        -device virtio-net-pci,netdev=net0 \
        -display none \
        -serial "file:$VM_LOG" \
        > "$SIGNALS/vm_stdout.log" 2>&1 &

    VM_PID=$!
    echo "$VM_PID" > "$PID_FILE"
    write_vm_status "running" "$VM_PID"

    echo "VM started (PID $VM_PID)."
    echo "  Serial log:   $VM_LOG"
    echo "  Stdout log:   $SIGNALS/vm_stdout.log"
    echo ""
    echo "Waiting for SSH port to open (up to 120s)..."
    i=0
    while [ $i -lt 24 ]; do
        sleep 5
        i=$((i+1))
        # nc -z checks TCP connectivity without authenticating — works regardless
        # of whether the VM uses password or key auth.
        if nc -z -w 3 127.0.0.1 "$SSH_PORT" 2>/dev/null; then
            echo "SSH port open (${i}x5s elapsed). VM is ready."
            write_vm_status "available" "$VM_PID"
            return
        fi
        printf "  %ds elapsed...\n" "$((i*5))"
    done
    echo "SSH port did not open within 120s. VM may still be booting."
    echo "Check $VM_LOG for boot progress."
}

# --- argument parsing ---

ACTION="start"
while [ "$#" -gt 0 ]; do
    case "$1" in
        --iso)    ISO="$2"; shift 2 ;;
        --ram)    RAM="$2"; shift 2 ;;
        --cpus)   CPUS="$2"; shift 2 ;;
        --ssh-port) SSH_PORT="$2"; shift 2 ;;
        --stop)   ACTION="stop"; shift ;;
        --status) ACTION="status"; shift ;;
        --start)  ACTION="start"; shift ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--iso PATH] [--ram 4G] [--cpus 2] [--ssh-port 2222]"
            echo "       $0 --stop | --status"
            exit 1
            ;;
    esac
done

case "$ACTION" in
    start)  cmd_start  ;;
    stop)   cmd_stop   ;;
    status) cmd_status ;;
esac
