# HogwartsLegacyMP

A open-source, community-built multiplayer mod for Hogwarts Legacy.

> **Early development** — not playable yet.

## Architecture

| Component | Technology | Description |
|-----------|-----------|-------------|
| Game mod | C++ / UE4SS | Reads game state, sends network packets |
| Lua research mods | UE4SS Lua | Exploration and prototyping |
| Server | C++ / ENet | Authoritative game server |
| Shared protocol | C++ | Packed message structs shared by client and server |

## Prerequisites

- [Hogwarts Legacy](https://store.steampowered.com/app/990080/Hogwarts_Legacy/) (Steam) — to run the mod
- [UE4SS v3.0.1](https://github.com/UE4SS-RE/RE-UE4SS/releases/tag/v3.0.1) — download and place at `UE4SS_v3.0.1-953-gb872ad11/`

**To build**, you need CMake 3.28+, Ninja, and a C++23 compiler. Either:
- **[Pixi](https://pixi.sh)** (recommended) — installs CMake, Ninja, and the compiler automatically via `pixi install`
- **Manual** — install CMake and Ninja yourself, plus:
  - Windows: Visual Studio 2022 with the C++ workload
  - Linux: GCC 13+ or Clang 16+
  - macOS: Xcode 15+ or Clang 16+

## Building

With Pixi:

```bash
pixi run build
```

Without Pixi (CMake directly):

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Clean rebuild with Pixi:

```bash
pixi run clean-build
```

Run tests (start the server first in a separate terminal):

```bash
pixi run server   # terminal 1
pixi run test     # terminal 2
```

## Deploying to the game

Set your game path in `scripts/deploy.sh` if it differs from the default, then:

```bash
# Deploy UE4SS + all Lua mods
./scripts/deploy.sh

# Deploy specific components
./scripts/deploy.sh --ue4ss    # UE4SS only
./scripts/deploy.sh --mods     # Lua mods only
./scripts/deploy.sh --client   # client DLL only (once built)
./scripts/deploy.sh --all      # everything
```

## Project Structure

```
HogwartsLegacyMP/
├── shared/          # Protocol.h — message structs shared by client and server
├── server/          # C++ ENet game server
├── mods/            # UE4SS Lua mods (research and prototyping)
├── tests/           # Catch2 unit + integration tests
├── scripts/         # deploy.sh and other tooling
└── .github/         # CI workflow (ubuntu-latest, runs on every push)
```

## Key Findings

Discovered via UE4SS reverse engineering:

| Class | Name |
|-------|------|
| PlayerController | `BP_Phoenix_Player_Controller_C` |
| Player Character | `BP_Biped_Player_C` |
| Character hierarchy | `BP_Biped_Player_C → Biped_Player → Biped_Character → Base_Character → ... → Character → Pawn → Actor` |

Position and rotation are readable via `K2_GetActorLocation()` / `K2_GetActorRotation()` on the pawn.

## Contributing

Branch off `main`, open a PR back to `main`. CI must pass before merging.
