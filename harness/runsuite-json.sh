#!/bin/sh
# harness/runsuite-json.sh — test runner for the dev container
#
# Runs the Vitruvian test suite against build artifacts in the named volume,
# emitting both human-readable and machine-readable (JSON) results.
#
# Environment variables:
#   VOS_SUITE    system | beapi | both  (default: both)
#   VOS_TIMEOUT  per-test timeout in seconds (default: 120)
#   VOS_BUILD_TYPE  Debug | Release (default: Debug)
set -e

WS=/workspace/ws
SIGNALS=/workspace/signals
LOGDIR="$SIGNALS/test-logs"
SUITE="${VOS_SUITE:-both}"
TIMEOUT_SEC="${VOS_TIMEOUT:-120}"
BUILD_DIR="$WS/generated"

mkdir -p "$LOGDIR"

UNITTESTER="$BUILD_DIR/src/tests/UnitTester"

# Test binary directories — paths mirror the source tree under $BUILD_DIR.
# CMAKE_RUNTIME_OUTPUT_DIRECTORY is not set in this project, so each executable
# lands in the cmake binary dir that corresponds to its CMakeLists.txt location.
TESTBIN_VOS="$BUILD_DIR/src/tests/vos/testharness"
TESTBIN_KERNEL="$BUILD_DIR/src/tests/system/kernel"
TESTBIN_KITS="$BUILD_DIR/src/tests/kits"
TESTBIN_SERVERS="$BUILD_DIR/src/tests/servers"

# Shared libs: collect every directory containing a .so in the build tree.
# Strip the trailing ':' to avoid adding CWD ('.') to the linker search path.
_so_dirs="$(find "$BUILD_DIR" -name '*.so' -exec dirname {} \; 2>/dev/null \
    | sort -u | tr '\n' ':' | sed 's/:$//')"
export LD_LIBRARY_PATH="${_so_dirs}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

TOTAL=0; PASS=0; FAIL=0; TIMEOUT_COUNT=0; SKIP=0
RESULTS_JSON=""

run_test() {
    name="$1"
    cmd="$2"
    logfile="$LOGDIR/${name}.log"

    if ! command -v sh > /dev/null 2>&1 || [ -z "$cmd" ]; then
        SKIP=$((SKIP+1))
        RESULTS_JSON="${RESULTS_JSON}{\"name\":\"${name}\",\"status\":\"skip\",\"exit_code\":-1,\"log\":\"\"},"
        return
    fi

    # Verify the first token of cmd exists as a binary reachable by PATH or
    # as an absolute path. Using 'command -v' handles both cases, and avoids
    # the broken '[ -x "rm" ]' CWD test that would fire for compound commands.
    binary=$(echo "$cmd" | awk '{print $1}')
    if ! command -v "$binary" > /dev/null 2>&1 && [ ! -x "$binary" ]; then
        SKIP=$((SKIP+1))
        RESULTS_JSON="${RESULTS_JSON}{\"name\":\"${name}\",\"status\":\"skip\",\"exit_code\":-1,\"log\":\"binary not found: ${binary}\"},"
        printf "  %-45s SKIP (no binary)\n" "$name"
        return
    fi

    TOTAL=$((TOTAL+1))
    printf "  %-45s" "$name"

    # 'set -e' would exit the whole script on a non-zero return from timeout.
    # Capture rc explicitly so a failing test does not abort the entire suite.
    rc=0
    timeout "$TIMEOUT_SEC" sh -c "$cmd" > "$logfile" 2>&1 || rc=$?

    if [ $rc -eq 124 ]; then
        TIMEOUT_COUNT=$((TIMEOUT_COUNT+1))
        FAIL=$((FAIL+1))
        status="timeout"
        echo "TIMEOUT"
    elif [ $rc -ne 0 ]; then
        FAIL=$((FAIL+1))
        status="fail"
        echo "FAIL (exit $rc)"
    elif grep -q "^- FAIL" "$logfile" 2>/dev/null; then
        FAIL=$((FAIL+1))
        status="fail"
        echo "FAIL"
    else
        PASS=$((PASS+1))
        status="pass"
        echo "PASS"
    fi

    # Escape log path for JSON
    RESULTS_JSON="${RESULTS_JSON}{\"name\":\"${name}\",\"status\":\"${status}\",\"exit_code\":${rc},\"log\":\"${logfile}\"},"
}

run_cppunit() {
    suite="$1"
    logname="$2"
    if [ -x "$UNITTESTER" ]; then
        run_test "$logname" "$UNITTESTER $suite"
    else
        SKIP=$((SKIP+1))
        RESULTS_JSON="${RESULTS_JSON}{\"name\":\"${logname}\",\"status\":\"skip\",\"exit_code\":-1,\"log\":\"UnitTester not built\"},"
        printf "  %-45s SKIP (UnitTester not built)\n" "$logname"
    fi
}

START_TS=$(date +%s)

echo "=========================================="
echo " Vitruvian Container Test Suite"
echo "=========================================="
echo "Build:      $BUILD_DIR"
echo "Suite:      $SUITE"
echo "Timeout:    ${TIMEOUT_SEC}s"
echo "Logs:       $LOGDIR"
echo ""

# ============================================================
# SYSTEM TESTS
# ============================================================
if [ "$SUITE" = "system" ] || [ "$SUITE" = "both" ]; then
    echo "------------------------------------------"
    echo " System Tests"
    echo "------------------------------------------"
    echo ""
    echo "  [VOS Testharness]"
    run_test "vos-testlist"             "$TESTBIN_VOS/testlist"
    run_test "vos-testoskit"            "$TESTBIN_VOS/testoskit"
    run_test "vos-testports"            "$TESTBIN_VOS/testports"
    run_test "vos-testports2"           "$TESTBIN_VOS/testports2"
    run_test "vos-testsem"              "$TESTBIN_VOS/testsem"
    run_test "vos-testsem2"             "$TESTBIN_VOS/testsem"
    run_test "vos-testsemdeletion"      "$TESTBIN_VOS/testsemdeletion"
    run_test "vos-testthread"           "$TESTBIN_VOS/testthread"
    run_test "vos-testteam"             "$TESTBIN_VOS/testteam"
    run_test "vos-testfsinfo"           "$TESTBIN_VOS/testfsinfo"
    run_test "vos-test_team_send_data"  "$TESTBIN_VOS/test_team_send_data"
    run_test "vos-set_port_owner"       "$TESTBIN_VOS/set_port_owner"
    run_test "vos-test_area"            "$TESTBIN_VOS/test_area"
    run_test "vos-testvref"             "$TESTBIN_VOS/testvref"
    run_test "vos-teststopwatch"        "$TESTBIN_VOS/teststopwatch"

    echo ""
    echo "  [System Kernel Tests]"
    run_test "kernel-fibo_load_image"       "$TESTBIN_KERNEL/fibo_load_image 20"
    run_test "kernel-path_resolution_test"  "$TESTBIN_KERNEL/path_resolution_test"
    run_test "kernel-port_close_test_1"     "$TESTBIN_KERNEL/port_close_test_1"
    run_test "kernel-port_close_test_2"     "$TESTBIN_KERNEL/port_close_test_2"
    run_test "kernel-port_delete_test"      "$TESTBIN_KERNEL/port_delete_test"
    run_test "kernel-port_multi_read_test"  "$TESTBIN_KERNEL/port_multi_read_test"
    run_test "kernel-port_wakeup_test_1"    "$TESTBIN_KERNEL/port_wakeup_test_1"
    run_test "kernel-port_wakeup_test_2"    "$TESTBIN_KERNEL/port_wakeup_test_2"
    run_test "kernel-port_wakeup_test_3"    "$TESTBIN_KERNEL/port_wakeup_test_3"
    run_test "kernel-port_wakeup_test_4"    "$TESTBIN_KERNEL/port_wakeup_test_4"
    run_test "kernel-port_wakeup_test_5"    "$TESTBIN_KERNEL/port_wakeup_test_5"
    run_test "kernel-port_wakeup_test_6"    "$TESTBIN_KERNEL/port_wakeup_test_6"
    run_test "kernel-port_wakeup_test_7"    "$TESTBIN_KERNEL/port_wakeup_test_7"
    run_test "kernel-port_wakeup_test_8"    "$TESTBIN_KERNEL/port_wakeup_test_8"
    run_test "kernel-port_wakeup_test_9"    "$TESTBIN_KERNEL/port_wakeup_test_9"

    echo ""
    echo "  [Support Kit System Tests]"
    run_test "support-string_utf8"      "$TESTBIN_KITS/support/string_utf8_tests"

    echo ""
    echo "  [App Kit System Tests]"
    run_test "app-portlink"             "$TESTBIN_KITS/app/messaging/PortLinkTest"
fi

# ============================================================
# BEAPI TESTS
# ============================================================
if [ "$SUITE" = "beapi" ] || [ "$SUITE" = "both" ]; then
    echo ""
    echo "------------------------------------------"
    echo " BeAPI Tests"
    echo "------------------------------------------"

    echo ""
    echo "  [Support Kit CppUnit]"
    run_cppunit "BArchivable"   "support-BArchivable"
    run_cppunit "BAutolock"     "support-BAutolock"
    run_cppunit "BDateTime"     "support-BDateTime"
    run_cppunit "BLocker"       "support-BLocker"
    run_cppunit "BMemoryIO"     "support-BMemoryIO"
    run_cppunit "BMallocIO"     "support-BMallocIO"
    run_cppunit "BString"       "support-BString"
    run_cppunit "BBlockCache"   "support-BBlockCache"
    run_cppunit "ByteOrder"     "support-ByteOrder"

    echo ""
    echo "  [Storage Kit CppUnit]"
    run_cppunit "BAppFileInfo"     "storage-BAppFileInfo"
    run_cppunit "BDirectory"       "storage-BDirectory"
    run_cppunit "BEntry"           "storage-BEntry"
    run_cppunit "BFile"            "storage-BFile"
    run_cppunit "BNode"            "storage-BNode"
    run_cppunit "BNodeInfo"        "storage-BNodeInfo"
    run_cppunit "BPath"            "storage-BPath"
    run_cppunit "BResources"       "storage-BResources"
    run_cppunit "BResourceStrings" "storage-BResourceStrings"
    run_cppunit "BSymLink"         "storage-BSymLink"
    run_cppunit "MimeSniffer"      "storage-MimeSniffer"

    echo ""
    echo "  [App Kit CppUnit]"
    run_cppunit "BApplication"   "app-BApplication"
    run_cppunit "BClipboard"     "app-BClipboard"
    run_cppunit "BHandler"       "app-BHandler"
    run_cppunit "BLooper"        "app-BLooper"
    run_cppunit "BMessageQueue"  "app-BMessageQueue"
    run_cppunit "BMessageRunner" "app-BMessageRunner"
    run_cppunit "BMessenger"     "app-BMessenger"
    run_cppunit "BPropertyInfo"  "app-BPropertyInfo"
    run_cppunit "BRoster"        "app-BRoster"

    echo ""
    echo "  [Interface Kit CppUnit]"
    run_cppunit "BPolygon"          "interface-BPolygon"
    run_cppunit "BRegion"           "interface-BRegion"
    run_cppunit "GraphicsDefs"      "interface-GraphicsDefs"
    run_cppunit "BAlert"            "interface-BAlert"
    run_cppunit "BBitmap"           "interface-BBitmap"
    run_cppunit "BDeskbar"          "interface-BDeskbar"
    run_cppunit "BMenu"             "interface-BMenu"
    run_cppunit "BOutlineListView"  "interface-BOutlineListView"
    run_cppunit "BTextControl"      "interface-BTextControl"
    run_cppunit "BTextView"         "interface-BTextView"

    echo ""
    echo "  [Shared Kit CppUnit]"
    run_cppunit "CalendarViewTest"    "shared-CalendarView"
    run_cppunit "NaturalCompareTest"  "shared-NaturalCompare"
    run_cppunit "JsonEndToEndTest"    "shared-JsonEndToEnd"
    run_cppunit "JsonErrorHandlingTest" "shared-JsonErrorHandling"
    run_cppunit "JsonTextWriterTest"  "shared-JsonTextWriter"
    run_cppunit "JsonToMessageTest"   "shared-JsonToMessage"
    run_cppunit "KeymapTest"          "shared-Keymap"

    echo ""
    echo "  [Storage Kit Runtime Tests]"
    run_test "storage-PathMonitorTest"  "$TESTBIN_KITS/storage/testapps/PathMonitorTest"
    run_test "storage-PathMonitorTest2" "rm -rf /tmp/path-monitor-test; $TESTBIN_KITS/storage/testapps/PathMonitorTest2"

    echo ""
    echo "  [Registrar Tests]"
    run_test "server-RegistrarTest1"  "$TESTBIN_SERVERS/registrar/RegistrarTest1"
fi

END_TS=$(date +%s)
DURATION=$((END_TS - START_TS))

echo ""
echo "=========================================="
echo " Test Suite Summary"
echo "=========================================="
printf "Total: %d  Passed: %d  Failed: %d  Skipped: %d  Timeout: %d\n" \
    "$TOTAL" "$PASS" "$FAIL" "$SKIP" "$TIMEOUT_COUNT"
echo "Duration: ${DURATION}s"
echo "=========================================="

# Overall status
if [ "$FAIL" -gt 0 ]; then
    OVERALL="fail"
elif [ "$TOTAL" -eq 0 ]; then
    OVERALL="skip"
else
    OVERALL="pass"
fi

# Strip trailing comma from results array, write JSON
RESULTS_JSON="${RESULTS_JSON%,}"
cat > "$SIGNALS/test_result.json" <<EOF
{
  "status": "$OVERALL",
  "suite": "$SUITE",
  "total": $TOTAL,
  "passed": $PASS,
  "failed": $FAIL,
  "skipped": $SKIP,
  "timeout": $TIMEOUT_COUNT,
  "duration_s": $DURATION,
  "log_dir": "$LOGDIR",
  "results": [${RESULTS_JSON}]
}
EOF

echo "Results written to $SIGNALS/test_result.json"

[ "$FAIL" -eq 0 ]
