# HogwartsLegacyMP

A open-source, community-built multiplayer mod for Hogwarts Legacy.

> **Early development — not yet playable.** There is no release to install yet. Follow the repo for updates.

---

## For Players

### Requirements

- [Hogwarts Legacy](https://store.steampowered.com/app/990080/Hogwarts_Legacy/) on Steam (PC)
- Windows 10/11

### Installation

_No release is available yet. When one is ready, installation will work as follows:_

1. Download the latest release zip from the [Releases](../../releases) page
2. Extract it into your Hogwarts Legacy install folder:
   ```
   <Steam>/steamapps/common/Hogwarts Legacy/Phoenix/Binaries/Win64/
   ```
3. Launch the game through Steam as normal
4. Connect to a server using the in-game menu

---

## For Developers

### Architecture

| Component | Technology | Description |
|-----------|-----------|-------------|
| Client mod | C++ / UE4SS Lua | Reads game state, sends position to server |
| Networking module | C++ (Lua extension DLL) | ENet UDP transport between game and server |
| Server | C++ / ENet | Authoritative game server |
| Shared protocol | C++ | Packed message structs shared by client and server |

### Prerequisites

- [UE4SS v3.0.1](https://github.com/UE4SS-RE/RE-UE4SS/releases/tag/v3.0.1) — extract into `UE4SS_v3.0.1-953-gb872ad11/`
- CMake 3.28+, Ninja, and a C++23 compiler. Either:
  - **[Pixi](https://pixi.sh)** (recommended) — run `pixi install` and it handles everything
  - **Manual**: Visual Studio 2022 (Windows), GCC 13+ or Clang 16+ (Linux/macOS)

### Building

```bash
pixi run build        # build everything
pixi run clean-build  # wipe build dir and rebuild from scratch
```

Without Pixi:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Running Tests

```bash
pixi run server   # terminal 1 — start the server
pixi run test     # terminal 2 — run all tests
```

### Deploying to the Game

Update the `GAME_DIR` path in `scripts/deploy.sh` if your install location differs, then:

```bash
./scripts/deploy.sh            # deploy UE4SS + mods (default)
./scripts/deploy.sh --ue4ss   # UE4SS only
./scripts/deploy.sh --mods    # Lua mods only
./scripts/deploy.sh --client  # client DLL only
./scripts/deploy.sh --all     # everything
```

### Project Structure

```
HogwartsLegacyMP/
├── shared/          # Protocol.h — message structs (client + server)
├── server/          # C++ ENet game server
├── client/          # HogwartsMPNet.dll — Lua C extension for networking
├── mods/            # UE4SS Lua mods
├── tests/           # Catch2 unit + integration tests
├── scripts/         # deploy.sh and tooling
└── .github/         # CI (ubuntu-latest, runs on every push)
```

### UE4SS Findings

Discovered via UE4SS reverse engineering:

| Class | Name |
|-------|------|
| PlayerController | `BP_Phoenix_Player_Controller_C` |
| Player Character | `BP_Biped_Player_C` |
| Character hierarchy | `BP_Biped_Player_C → Biped_Player → Biped_Character → Base_Character → ... → Character → Pawn → Actor` |

Position and rotation: `K2_GetActorLocation()` / `K2_GetActorRotation()` on the pawn.

### Contributing

Branch off `main`, open a PR back to `main`. CI must pass before merging.
