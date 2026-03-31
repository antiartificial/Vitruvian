#!/bin/bash
# Revert the absolute-path #include hacks that were applied to work around
# macOS case-insensitive filesystem + Docker bind-mount.
# Run this ONCE after migrating the project to VitruvianDev.sparsebundle.
#
# Safe to run: these changes are mechanical and idempotent.
# Do NOT run on the original ~/projects/Vitruvian (still on case-insensitive FS).

set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
echo "Reverting case-insensitivity workarounds in: $REPO"

# 1. All 42 C files: restore #include "/usr/include/string.h" -> #include <string.h>
echo "  Reverting .c files..."
find "$REPO/src" -name "*.c" -print0 | xargs -0 \
    sed -i '' 's|#include "/usr/include/string\.h"|#include <string.h>|g'

# 2. LinuxBuildCompatibility.h
echo "  Reverting LinuxBuildCompatibility.h..."
sed -i '' 's|#include "/usr/include/string\.h"|#include <string.h>|g' \
    "$REPO/headers/build/LinuxBuildCompatibility.h"

# 3. system_revision.c (already covered by .c glob above, this is a no-op safety net)

# 4. CMakeLists.txt: remove the force-include block
echo "  Reverting CMakeLists.txt force-include..."
python3 - "$REPO/CMakeLists.txt" <<'EOF'
import sys, re

path = sys.argv[1]
with open(path) as f:
    content = f.read()

block = (
    r'# GCC 14 / macOS case-insensitive bind mount workaround:[^\n]*\n'
    r'# Force-include the system string\.h[^\n]*\n'
    r'# etc\.[^\n]*\n'
    r'if\(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux"\)\n'
    r'    # Prepend[^\n]*\n'
    r'    set\(CMAKE_C_FLAGS[^\n]*\n'
    r'    set\(CMAKE_CXX_FLAGS[^\n]*\n'
    r'endif\(\)\n'
)
new_content = re.sub(block, '', content)
if new_content == content:
    # Try simpler pattern
    block2 = (
        r'# GCC 14 / macOS case-insensitive bind mount workaround:\n'
        r'.*?\nendif\(\)\n'
    )
    new_content = re.sub(block2, '', content, flags=re.DOTALL)

if new_content != content:
    with open(path, 'w') as f:
        f.write(new_content)
    print("    Removed force-include block from CMakeLists.txt")
else:
    print("    Block not found (may already be reverted) — skipping")
EOF

# 5. headers/build/os/support/String.h shadow: the #ifdef __cplusplus guard is
#    CORRECT to keep (C code should never include BString). Only revert the
#    absolute path back to #include_next which works on case-sensitive FS.
echo "  Updating shadow String.h to use #include_next..."
cat > "$REPO/headers/build/os/support/String.h" << 'SHADOW'
/*
 * Build-system shadow for <string.h>.
 * On a case-sensitive filesystem, #include_next correctly reaches the real
 * system string.h. The C++ BString class is guarded so C TUs are unaffected.
 */
#include_next <string.h>

#ifdef __cplusplus
#  ifndef _BUILD_BSTRING_SHADOW_INCLUDED
#  define _BUILD_BSTRING_SHADOW_INCLUDED
#    include <../os/support/String.h>
#  endif
#endif
SHADOW

echo ""
echo "Done. Workarounds reverted."
echo "Run ./harness/run.sh to verify the build is green."
