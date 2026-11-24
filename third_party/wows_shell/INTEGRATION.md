# Bundled wows_shell reference implementation

Upstream: https://github.com/jcw780/wows_shell (jcw780)
License: MIT, see LICENSE, copyright (c) 2020 jcw780.
Source files copied unchanged from the upstream master branch (2026-11-24).
The adapter and tests are authored for OceanGambit and are not upstream files.

Included: shell.hpp, shellCPP.hpp, utility.hpp, controlEnums.hpp.
Not included: Python bindings, WebAssembly, fitShell, upstream tests,
CSV datasets, or the optional version2 SIMD submodule.

**SIMD caveat:** shellCPP.hpp conditionally includes `version2` when
`__SSE4_1__` or `__AVX__` is defined. We deliberately do not enable those
modes for the standalone WowsShellCheck target. If enabling SIMD later,
vendor or link the version2 library separately and comply with its
Apache-2.0 license (https://github.com/vectorclass/version2).

No game assets, proprietary binaries or recovered client source are bundled.
wows_shell's empirical WoWS penetration and game-rule routines remain upstream
source and are not gameplay decisions in OceanGambit.

Integration:
- battle/WowsShellAdapter.h: independent projectile inputs/results.
- battle/WowsShellAdapter.cpp: invokes the upstream calculator with one worker.
- tests/WowsShellCheck.cpp: independent smoke checks, does not require OSG.
- CMakeLists.txt: builds WowsShellCheck only when BUILD_TESTING is enabled.
- The actual BattleScene/ProjectileSystem remain unchanged; no gameplay switch.
- Actual time `timeToTarget` is used, not `timeToTargetAdjusted`.
- Penetration value is provided for reference only. Armor, fuse, normalization,
  ricochet, dispersion and sigma calculations are not yet wired up.
- This is a comparison tool, NOT a directly integrated 3D ballistic engine.
  Its trajectory model and terminal-plane conventions must be reconciled
  with OceanGambit's 3D coordinate system before live use.

For future work: independently test float precision, coordinate units,
sweep versus per-projectile runtime performance, and reference consistency
against a fixed-step solver before using the outputs for FireControl.
