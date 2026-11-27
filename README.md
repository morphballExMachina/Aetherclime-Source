# Aetherclime

**An environmental intelligence framework for Skyrim Special Edition / Anniversary Edition (SKSE).**

*Aetherclime understands the environment. Other mods bring it to life.*

Aetherclime is a read-only native SKSE framework providing calendar seasons, regional climate, available weather and geographic context, modeled outdoor and indoor temperatures, and a versioned C API for other plugins. Its estimates include availability and provenance metadata so consumers can make informed decisions.

**0.5.0 RC1** is a release candidate. This repository contains source and developer-facing materials, **not** an installable Skyrim DLL. The runtime FOMOD is distributed separately on Nexus. Broader performance and modlist compatibility testing is still in progress.

## Features

- No ESP, ESL, or ESM plugin slot required by the framework.
- Calendar seasons independent of Seasons of Skyrim's visual asset swaps.
- Seasonal regional temperatures informed by time, weather, and geography.
- Generic ambient estimates for occupied buildings, ruins, caves, mines, and other interiors.
- Lower-confidence geographic fallbacks with explicit estimate provenance.
- Versioned V1/V2/V3 environmental snapshots and a bounded condition-query API.
- Optional HUD; silent by default. The framework does **not** directly alter weather, NPC behavior, vegetation, meshes, LODs, or shaders.

## Developer documentation

- [Native API reference](docs/release/API_REFERENCE.md)
- [API quick start](docs/release/API_QUICK_START.md)
- [Climate model in plain English](docs/release/CLIMATE_MODEL_PLAIN_ENGLISH.md)
- [Configuration](docs/release/CONFIGURATION.md)
- [Installation and compatibility](docs/release/INSTALL_COMPATIBILITY.md)
- [Troubleshooting](docs/release/TROUBLESHOOTING.md)
- [Bug reporting](docs/release/BUG_REPORTING.md)

For C++ consumers, `include/Aetherclime/PluginApi.h` is the authoritative C ABI declaration. `include/Aetherclime/NativeClient.h` is an optional client helper. Consumers should discover optional exports instead of assuming availability, call the API on the Skyrim game thread, and check availability flags.

## Building

Requirements: Windows x64, Visual Studio C++ build tools, CMake, Ninja, vcpkg, and a compatible SKSE setup. The intended vcpkg triplet is `x64-windows-static-md`.

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows-static-md -DAETHERCLIME_BUILD_SKSE=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The release's binary packaging script is `tools/Package-AetherclimeRC1.ps1`. It requires a locally built Windows DLL and copies verified third-party copyright notices from installed vcpkg packages. Source SDK users do not need to run that script to consume the public headers.

## Repository layout

- `src/` — implementation
- `include/Aetherclime/` — public and internal headers
- `tests/` — portable regression tests
- `examples/` — external consumer examples
- `docs/release/` — user and developer documentation
- `Data/` and `packaging/` — default configuration and FOMOD templates

## License and acknowledgments

Original Aetherclime code and documentation are offered under the [MIT License](LICENSE). Third-party dependencies retain their own licenses; see [Third-party dependencies](THIRD_PARTY_DEPENDENCIES.md). The user-facing FOMOD includes the original copyright notices for its resolved, statically linked library dependencies.

Aetherclime has been developed with substantial generative-AI assistance in code implementation, tests, and documentation, directed and playtested by its maintainer. Technical feedback and reproducible issue reports are welcome.
