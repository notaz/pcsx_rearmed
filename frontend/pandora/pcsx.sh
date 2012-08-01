#!/bin/sh

# stupid nub mode thing
nub0mode=`cat /proc/pandora/nub0/mode`
nub1mode=`cat /proc/pandora/nub1/mode`
/usr/pandora/scripts/op_nubchange.sh absolute absolute

./pcsx "$@"

# restore stuff if pcsx crashes
./picorestore
sudo -n /usr/pandora/scripts/op_lcdrate.sh 60

/usr/pandora/scripts/op_nubchange.sh $nub0mode $nub1mode
