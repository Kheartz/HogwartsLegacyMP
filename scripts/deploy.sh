#!/usr/bin/env bash
set -euo pipefail

GAME_DIR="/d/Program Files (x86)/Steam/steamapps/common/Hogwarts Legacy/Phoenix/Binaries/Win64"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE4SS_SRC="$REPO_ROOT/UE4SS_v3.0.1-953-gb872ad11"

echo "=== Hogwarts Legacy MP Deploy ==="
echo "Game dir: $GAME_DIR"
echo "Repo:     $REPO_ROOT"
echo ""

# ── UE4SS ──────────────────────────────────────────────────────────────────
if [[ "${1:-}" == "--ue4ss" || "${1:-}" == "--all" || $# -eq 0 ]]; then
    echo "[ue4ss] Copying dwmapi.dll..."
    cp "$UE4SS_SRC/dwmapi.dll" "$GAME_DIR/dwmapi.dll"

    echo "[ue4ss] Copying ue4ss/ folder..."
    cp -r "$UE4SS_SRC/ue4ss/." "$GAME_DIR/ue4ss/"

    echo "[ue4ss] Done."
fi

# ── Client DLL ─────────────────────────────────────────────────────────────
# Uncomment when the client DLL build exists:
# CLIENT_DLL="$REPO_ROOT/build/HogwartsMP.dll"
# if [[ "${1:-}" == "--client" || "${1:-}" == "--all" ]]; then
#     if [[ ! -f "$CLIENT_DLL" ]]; then
#         echo "[client] ERROR: $CLIENT_DLL not found. Build first."
#         exit 1
#     fi
#     echo "[client] Copying HogwartsMP.dll..."
#     cp "$CLIENT_DLL" "$GAME_DIR/HogwartsMP.dll"
#     echo "[client] Done."
# fi

# ── Lua Mods (UE4SS scripts) ───────────────────────────────────────────────
MODS_SRC="$REPO_ROOT/mods"
MODS_DST="$GAME_DIR/ue4ss/Mods"
if [[ "${1:-}" == "--mods" || "${1:-}" == "--all" || $# -eq 0 ]]; then
    echo "[mods] Syncing Lua mods..."
    cp -r "$MODS_SRC/." "$MODS_DST/"
    echo "[mods] Done."
fi

echo ""
echo "=== Deploy complete ==="
echo ""
echo "Usage:"
echo "  ./scripts/deploy.sh            # deploy UE4SS (default)"
echo "  ./scripts/deploy.sh --ue4ss   # deploy UE4SS only"
echo "  ./scripts/deploy.sh --client  # deploy client DLL only"
echo "  ./scripts/deploy.sh --mods    # deploy Lua mods only"
echo "  ./scripts/deploy.sh --all     # deploy everything"
