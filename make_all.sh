#!/bin/sh
ninja -C build && ./tools/copy_assets.sh && cmake --build build-pd && cmake --build build-pd-device
