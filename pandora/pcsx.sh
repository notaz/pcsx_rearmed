#!/bin/sh

# stupid nub mode thing
nub0mode=`cat /proc/pandora/nub0/mode`
nub1mode=`cat /proc/pandora/nub1/mode`
echo absolute > /proc/pandora/nub0/mode
echo absolute > /proc/pandora/nub1/mode

./pcsx "$@"

# restore stuff if pcsx crashes
./picorestore

echo "$nub0mode" > /proc/pandora/nub0/mode
echo "$nub1mode" > /proc/pandora/nub1/mode
