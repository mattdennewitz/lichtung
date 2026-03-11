# Phase 5: Validation and Universal Build - Research

**Researched:** 2026-03-11
**Domain:** macOS universal binary builds, plugin format validation, DSP A/B testing
**Confidence:** HIGH

## Summary

Phase 5 converts the existing arm64-only JUCE plugin into a macOS universal binary (arm64 + x86_64), validates both VST3 and AU formats using pluginval and auval, and performs A/B testing against the Max/MSP reference implementation. The project already builds and runs correctly on Apple Silicon -- the universal binary change is a single CMake variable. Validation requires installing pluginval and running both it and the system auval tool. A/B testing is a manual comparison process.

The current CMakeLists.txt has no `CMAKE_OSX_ARCHITECTURES` set (defaults to host arch only). The build uses Makefiles (not Xcode generator), which fully supports multi-arch builds via `-arch` flags. No code changes are expected -- this phase is purely build configuration and verification.

**Primary recommendation:** Set `CMAKE_OSX_ARCHITECTURES` to `arm64;x86_64` in CMakeLists.txt, do a clean rebuild, verify with `file` and `lipo`, then run pluginval and auval against the output artefacts.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| FMT-03 | macOS universal binary (Apple Silicon + Intel) | Set CMAKE_OSX_ARCHITECTURES="arm64;x86_64", verify with lipo -archs. See Architecture Patterns section. |
</phase_requirements>

## Standard Stack

### Core
| Tool | Version | Purpose | Why Standard |
|------|---------|---------|--------------|
| CMake | 3.22+ (project minimum) | Build system | Already in use, CMAKE_OSX_ARCHITECTURES is the standard mechanism |
| JUCE | 8.0.12 | Plugin framework | Already in use via FetchContent |
| pluginval | v1.0.4 | Plugin format validation | Industry standard for JUCE plugin validation |
| auval | 1.10.0 (system) | AU format validation | Apple's official AU validation tool, already installed at /usr/bin/auval |

### Supporting
| Tool | Purpose | When to Use |
|------|---------|-------------|
| `lipo -archs` | Verify universal binary contains both slices | After build, before validation |
| `file` command | Quick arch check on binary | Smoke test after build |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Makefile generator | Xcode generator | Xcode generator also works for universal builds but adds complexity; Makefile is simpler and already working |
| pluginval GUI | pluginval CLI | CLI is better for automation and clear pass/fail |

## Architecture Patterns

### CMake Universal Binary Configuration

The change is minimal. Add one line before `FetchContent_Declare`:

```cmake
# Universal binary - must be set BEFORE any project() or target definitions
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
```

**Placement matters:** `CMAKE_OSX_ARCHITECTURES` should be set early in CMakeLists.txt, before `project()` or at minimum before any targets are defined. Setting it after target creation has no effect on those targets.

**Deployment target:** Setting `CMAKE_OSX_DEPLOYMENT_TARGET` to `11.0` is recommended for universal builds since macOS 11 Big Sur was the first release supporting Apple Silicon. Without this, the x86_64 slice may target an older macOS that never had ARM support, which is technically fine but inconsistent.

```cmake
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")
```

### Clean Rebuild Required

After changing architecture settings, a clean rebuild is mandatory. The CMake cache stores architecture info:

```bash
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Verification Commands

```bash
# Check standalone binary
file build/HarmonicOscillator_artefacts/Release/Standalone/Harmonic\ Oscillator.app/Contents/MacOS/Harmonic\ Oscillator
# Expected: Mach-O universal binary with 2 architectures: [x86_64:Mach-O 64-bit executable x86_64] [arm64]

# Check AU component
lipo -archs build/HarmonicOscillator_artefacts/Release/AU/Harmonic\ Oscillator.component/Contents/MacOS/Harmonic\ Oscillator
# Expected: x86_64 arm64

# Check VST3
lipo -archs build/HarmonicOscillator_artefacts/Release/VST3/Harmonic\ Oscillator.vst3/Contents/MacOS/Harmonic\ Oscillator
# Expected: x86_64 arm64
```

### Anti-Patterns to Avoid
- **Setting CMAKE_OSX_ARCHITECTURES via command line only:** Embed it in CMakeLists.txt so every build is universal by default. Command-line override remains possible.
- **Forgetting clean rebuild:** Incremental builds with changed arch settings produce broken binaries.
- **Not setting deployment target:** The x86_64 slice could target an inconsistent macOS version.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Plugin format validation | Custom test harness | pluginval CLI | Tests hundreds of edge cases (parameter fuzzing, state save/restore, threading) |
| AU validation | Manual testing in DAW | auval (system tool) | Apple's official conformance test |
| Arch verification | Custom scripts | `lipo -archs` / `file` | Standard macOS tools, definitive answers |

**Key insight:** Plugin validation is a solved problem. pluginval and auval together cover format conformance comprehensively. Manual DAW testing is for user experience, not format correctness.

## Common Pitfalls

### Pitfall 1: Stale Build Cache
**What goes wrong:** Changing CMAKE_OSX_ARCHITECTURES without cleaning the build directory produces arm64-only binaries.
**Why it happens:** CMake caches architecture settings; incremental builds don't pick up the change.
**How to avoid:** Always `rm -rf build` before reconfiguring after arch changes.
**Warning signs:** `lipo -archs` shows only one architecture.

### Pitfall 2: pluginval Not Installed
**What goes wrong:** pluginval is not a system tool -- it must be downloaded separately.
**Why it happens:** Unlike auval (bundled with macOS), pluginval is a third-party tool.
**How to avoid:** Download from GitHub releases before running validation.
**Warning signs:** `which pluginval` returns nothing.

### Pitfall 3: AU Component Not Registered
**What goes wrong:** auval cannot find the plugin because it hasn't been copied to ~/Library/Audio/Plug-Ins/Components/.
**Why it happens:** COPY_PLUGIN_AFTER_BUILD is TRUE in CMakeLists.txt, but only copies to the standard location after a successful build. If the AU component folder doesn't exist or permissions are wrong, copy fails silently.
**How to avoid:** After build, verify the .component file exists in ~/Library/Audio/Plug-Ins/Components/ before running auval. Use `auval -a` to list registered AUs.
**Warning signs:** auval reports "AudioUnit not found."

### Pitfall 4: pluginval Strictness Level Too Low
**What goes wrong:** Validation passes but plugin still has issues in real hosts.
**Why it happens:** Low strictness levels (1-4) only test basic crash detection.
**How to avoid:** Use strictness level 5 minimum (recognized standard for host compatibility). Level 10 for thorough testing.
**Warning signs:** Very fast validation run (< 5 seconds).

### Pitfall 5: VST3 Path Trailing Slash
**What goes wrong:** pluginval fails to load VST3 if path has trailing slash.
**Why it happens:** Known pluginval issue (GitHub issue #20).
**How to avoid:** Don't include trailing slash in the .vst3 bundle path.

## Code Examples

### pluginval Installation and Usage

```bash
# Download pluginval (macOS)
curl -L "https://github.com/Tracktion/pluginval/releases/download/latest_release/pluginval_macOS.zip" -o /tmp/pluginval.zip
unzip -o /tmp/pluginval.zip -d /tmp/pluginval
# The app bundle is at /tmp/pluginval/pluginval.app
# CLI binary is inside: /tmp/pluginval/pluginval.app/Contents/MacOS/pluginval

# Validate VST3 (strictness 5 = minimum for host compatibility)
/tmp/pluginval/pluginval.app/Contents/MacOS/pluginval \
  --strictness-level 5 \
  --validate "build/HarmonicOscillator_artefacts/Release/VST3/Harmonic Oscillator.vst3"

# Validate AU
/tmp/pluginval/pluginval.app/Contents/MacOS/pluginval \
  --strictness-level 5 \
  --validate "build/HarmonicOscillator_artefacts/Release/AU/Harmonic Oscillator.component"
```

### auval Usage

```bash
# Validate AU using component IDs from CMakeLists.txt
# TYPE=aumu (AU Music Device / Synth), SUBT=Hrmo, MANU=DsEr
auval -v aumu Hrmo DsEr

# List all installed AUs to verify plugin is registered
auval -a | grep -i harmonic
```

### lipo Verification

```bash
# Verify both architectures present
lipo -archs "build/HarmonicOscillator_artefacts/Release/VST3/Harmonic Oscillator.vst3/Contents/MacOS/Harmonic Oscillator"
# Expected output: x86_64 arm64

# Detailed info
lipo -detailed_info "build/HarmonicOscillator_artefacts/Release/VST3/Harmonic Oscillator.vst3/Contents/MacOS/Harmonic Oscillator"
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Separate x86_64 and arm64 builds | Single CMAKE_OSX_ARCHITECTURES="arm64;x86_64" | macOS 11 / CMake 3.19+ | One build produces both slices |
| Manual DAW testing for validation | pluginval CLI (v1.0.4) | 2024 (1.0 release) | Automated, CI-friendly validation |
| pluginval does not run auval | pluginval runs auval at strictness > 5 | v1.0+ | Single tool covers both VST3 and AU validation |

## A/B Testing Against Max/MSP Reference

This is inherently a manual/human process since it requires running both the Max/MSP patch and the JUCE plugin simultaneously. The success criteria call for comparison at 44.1k, 48k, and 96k sample rates across harmonic mix, scan, tilt, and PolyBLEP waveforms.

**Approach:** The planner should treat this as a human verification checkpoint (similar to Phase 4's GUI approval). The user compares output by ear or by recording both outputs and comparing waveforms visually.

**What to test:**
1. Harmonic mix with various harmonic level combinations
2. Scan center sweep with different widths
3. Tilt at extremes (+24dB, -24dB) and center (0dB)
4. Each PolyBLEP waveform (saw, square, triangle) across octaves
5. All of the above at 44.1k, 48k, and 96k sample rates

**Not automatable in this context:** Without the Max/MSP reference output as recorded audio files or numerical data, automated comparison is not feasible. This is a subjective/manual verification step.

## Open Questions

1. **pluginval strictness level preference**
   - What we know: Level 5 is the minimum standard for host compatibility. Level 10 is maximum thoroughness. pluginval v1.0+ runs auval automatically at levels > 5.
   - What's unclear: User's preference for strictness level.
   - Recommendation: Use level 5 for initial validation, escalate to 10 if time permits. Since pluginval > 5 runs auval too, use level 10 and skip separate auval run.

2. **A/B testing methodology**
   - What we know: User has the Max/MSP gen~ patch as reference.
   - What's unclear: Whether user wants quantitative (waveform diff) or qualitative (listen and compare) testing.
   - Recommendation: Plan as a human verification checkpoint. Provide a checklist of what to compare.

## Sources

### Primary (HIGH confidence)
- [CMake documentation](https://cmake.org/cmake/help/latest/variable/CMAKE_OSX_ARCHITECTURES.html) - CMAKE_OSX_ARCHITECTURES variable
- [pluginval GitHub](https://github.com/Tracktion/pluginval) - v1.0.4, CLI usage, strictness levels
- System auval (v1.10.0) - verified installed at /usr/bin/auval
- Current build inspection - confirmed arm64-only, Makefile generator, artefact paths

### Secondary (MEDIUM confidence)
- [JUCE Forum: CMAKE plugin and OS 11 Universal Binary](https://forum.juce.com/t/cmake-plugin-and-os-11-universal-binary/41997) - universal binary configuration with JUCE CMake
- [Apple: Building a universal macOS binary](https://developer.apple.com/documentation/apple-silicon/building-a-universal-macos-binary) - official Apple documentation
- [pluginval GitHub Issue #20](https://github.com/Tracktion/pluginval/issues/20) - VST3 trailing slash issue

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - CMake universal binary is well-documented, pluginval/auval are standard tools
- Architecture: HIGH - single CMake variable change, verified current build state
- Pitfalls: HIGH - well-known issues from JUCE forum and pluginval GitHub
- A/B testing: MEDIUM - methodology depends on user's available reference setup

**Research date:** 2026-03-11
**Valid until:** 2026-04-11 (stable domain, tools don't change frequently)
