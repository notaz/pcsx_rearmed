#!/bin/sh

./pcsx "$@"

# restore stuff if pcsx crashes
./picorestore
