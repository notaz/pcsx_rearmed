#!/bin/sh

# stupid nub mode thing
nub0mode=`cat /proc/pandora/nub0/mode`
nub1mode=`cat /proc/pandora/nub1/mode`
/usr/pandora/scripts/op_nubchange.sh absolute absolute

# 4MB for RAM (2+align) + 2MB for vram (1+overdraw)
#  + 10MB for gpu_neon (8+overdraw) + 8MB LUTs
# no big deal if this fails, only performance loss
sudo -n /usr/pandora/scripts/op_hugetlb.sh 24

# C64x DSP for SPU
sudo -n /usr/pandora/scripts/op_dsp_c64.sh

./pcsx "$@"

# restore stuff if pcsx crashes
./picorestore
sudo -n /usr/pandora/scripts/op_lcdrate.sh 60
sudo -n /usr/pandora/scripts/op_gamma.sh 0
sudo -n /usr/pandora/scripts/op_hugetlb.sh 0

/usr/pandora/scripts/op_nubchange.sh $nub0mode $nub1mode
