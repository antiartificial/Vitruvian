# Vitruvian Development Notes

## Prerequisites

- Docker Desktop (or Docker Engine on Linux)
- Git
- On macOS: a **case-sensitive** APFS volume (see below)

## macOS Setup — Case-Sensitive Volume Required

The Vitruvian source tree contains both `String.h` (BString C++ class) and
code that includes `<string.h>` (C standard library). On the default
case-insensitive APFS filesystem these resolve to the same file, breaking
the build.

Create a dedicated case-sensitive sparse bundle once:

```sh
hdiutil create -size 30g -fs "Case-sensitive APFS" \
  -volname VitruvianDev ~/VitruvianDev.sparsebundle
hdiutil attach ~/VitruvianDev.sparsebundle
```

Then clone (or copy) the repo onto that volume:

```sh
git clone https://github.com/VitruvianOS/Vitruvian.git \
  /Volumes/VitruvianDev/Vitruvian
```

Convenience mount script (`~/mount-vitruvian.sh`):

```sh
#!/bin/bash
hdiutil attach ~/VitruvianDev.sparsebundle -mountpoint /Volumes/VitruvianDev
```

On **Linux** no special setup is needed — `ext4` and most Linux filesystems
are case-sensitive by default.

## Building

The build runs entirely inside a Docker container:

```sh
# Build the image (first time / after Dockerfile.dev changes)
docker compose build

# Full build + test suite
./harness/run.sh

# Build only
docker compose run --rm builder
```

Build artefacts land in the `vitruvian-build` named Docker volume and are
not written back to the source tree.

### Harness signals

`harness/signals/status.json` is written after every run:

```json
{ "signal": "green", "total": 81, "pass": 27, "fail": 54 }
```

`signal` values: `green` (all pass), `yellow` (some fail), `red` (build
failed).

## GCC 14 Compatibility

GCC 14 (Debian Trixie, used in the dev container) no longer implicitly
includes `<cstring>` through other headers. The following changes were
applied to fix the cascade of `memcpy`/`strlen`/`strcmp` not-declared errors:

- Added `#include <cstring>` to ~20 files in `headers/libs/agg/`,
  `headers/private/`, and `src/` where string functions were used without
  an explicit include.
- `headers/os/support/String.h`: replaced bare `strcmp` in inline comparison
  operators with `__builtin_strcmp` so the operators are immune to missing
  `<string.h>` includes in translation units that include `String.h`.
- `headers/build/LinuxBuildCompatibility.h`: added `#include <sys/cdefs.h>`
  and `__THROW` to `strlcpy`/`strlcat` declarations to match glibc 2.38.
- `headers/build/os/support/String.h`: shadow header using `#include_next`
  so that `#include <String.h>` (capital S) resolves correctly on
  case-sensitive filesystems.

## Nexus Stub Layer (Tier-0 Builds)

Vitruvian has two build tiers:

| Tier | Environment | nexus kernel module |
|------|-------------|---------------------|
| 0    | Docker / CI | **absent** — stubs return `B_NOT_SUPPORTED` |
| 1    | VM / bare metal | present — real kernel primitives |

`src/system/libroot2/nexus_stubs.cpp` provides stub implementations of all
kernel-primitive APIs (threads, semaphores, ports, areas, team info, vnode
refs) so that `libroot.so` and the full application tree link cleanly in
Tier-0. At runtime these stubs cause tests that exercise kernel primitives
to fail gracefully instead of producing linker errors.

The CMake guard in `src/system/libroot2/CMakeLists.txt` selects either the
real nexus-backed sources or the stub file:

```cmake
if(TARGET nexus)
    target_sources(root PRIVATE Team.cpp thread.cpp port.cpp ...)
else()
    target_sources(root PRIVATE nexus_stubs.cpp)
endif()
```

## Build System Notes

### Link order

`libbe.so` depends on symbols from `libroot.so`. In `build/engine.cmake` the
`Application`, `Server`, and `Test` macros prepend libraries in this order:
`be` first, then `root`, so that the linker sees `be → root` and can resolve
`be`'s undefined references from `root`.

### Shared library undefined symbols

`libbe.so` intentionally has unresolved `_kern_*` references that are
satisfied by `libroot.so` at runtime. The global linker flag
`-Wl,--allow-shlib-undefined` (Linux only, set in the top-level
`CMakeLists.txt`) suppresses the link-time error for shared libraries; the
symbols are resolved via the embedded `RPATH` at process startup.

### ARM64 / Apple Silicon

The Docker container runs as `linux/arm64` on Apple Silicon. The `cpuid_info`
type and `get_cpuid()` function are x86-only and are guarded with:

```cpp
#if defined(__i386__) || defined(__x86_64__)
```

in `src/system/libroot2/compat/system_info.cpp`.

## Test Results (Tier-0 baseline)

27/81 tests pass. The 54 failures are all runtime-environment issues:

- **SIGABRT** (45 tests) — `create_sem()` returns `B_NOT_SUPPORTED` → every
  CppUnit test that uses `SemaphoreSyncObject` aborts immediately.
- **Exit 255** (6 tests) — tests that call `BLooper`/`BLocker` or load kernel
  images fail because those paths ultimately call nexus stubs.
- **Exit 1** (3 tests) — misc runtime errors.

All 54 are expected to pass on a Tier-1 build with nexus present.
