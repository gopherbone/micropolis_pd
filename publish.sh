#!/bin/sh

cmake --build build
cmake --build build_device
rm -rf micropolis.pdx
mv micropolis_DEVICE.pdx micropolis.pdx
rm micropolis.zip
zip micropolis.zip -r ./micropolis.pdx
