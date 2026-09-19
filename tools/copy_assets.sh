#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

SRC_DIR="$PROJECT_DIR/assets"
DST_DIR="$PROJECT_DIR/Source/assets"

mkdir -p "$DST_DIR"
cp -ru "$SRC_DIR/"* "$DST_DIR/"

echo "Copied assets to Source/assets successfully."
