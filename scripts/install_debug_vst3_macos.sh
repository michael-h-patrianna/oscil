#!/usr/bin/env zsh
set -euo pipefail

BUILD_DIR="${1:-build}"
CONFIG="Debug"
NAME="Oscil.vst3"
SRC_DIR="$PWD/${BUILD_DIR}/oscil_plugin_artefacts/${CONFIG}/VST3/${NAME}"
DEST_DIR="/Library/Audio/Plug-Ins/VST3/${NAME}"

if [[ ! -d "$SRC_DIR" ]]; then
  echo "Building Debug VST3..."
  cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=$CONFIG
  cmake --build "$BUILD_DIR" --config $CONFIG --target oscil_plugin_VST3 -- -j 4
fi

echo "Installing $NAME to /Library/Audio/Plug-Ins/VST3 (sudo may prompt for password)..."
sudo rm -rf "$DEST_DIR"
sudo mkdir -p "/Library/Audio/Plug-Ins/VST3"
sudo cp -a "$SRC_DIR" "/Library/Audio/Plug-Ins/VST3/"

echo "Installed: $DEST_DIR"
