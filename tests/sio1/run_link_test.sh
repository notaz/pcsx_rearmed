#!/bin/sh
# Runs the two process link test: the reference once, then the linked pair
# several times with different host timing. All results must be identical.
# usage: run_link_test.sh [seed] [cycles] [runs] [line delay]
set -e
bin=${TEST_LINK_BIN:-./test_link}
seed=${1:-1}
cycles=${2:-67737600}   # 2 emulated seconds
runs=${3:-4}
delay=${4:-0}
dir=$(mktemp -d)
trap 'rm -rf "$dir"' EXIT
export PCSX_SIO1_LINK_DIR="$dir"

"$bin" ref "$seed" "$cycles" "$delay" > "$dir/ref"
cat "$dir/ref"
fail=0
i=0
while [ $i -lt "$runs" ]; do
	name="t$i"
	if [ $i -eq 0 ]; then
		# no jitter, and B first
		"$bin" B $name "$seed" "$cycles" "$delay" > "$dir/b" 2> "$dir/berr" &
		"$bin" A $name "$seed" "$cycles" "$delay" > "$dir/a" 2> "$dir/aerr"
	else
		"$bin" A $name "$seed" "$cycles" "$delay" $i > "$dir/a" 2> "$dir/aerr" &
		sleep 0.0$i
		"$bin" B $name "$seed" "$cycles" "$delay" $((i + 100)) > "$dir/b" 2> "$dir/berr"
	fi
	wait
	cat "$dir/a" "$dir/b" > "$dir/ab"
	if cmp -s "$dir/ref" "$dir/ab"; then
		echo "run $i: identical to the reference"
	else
		echo "run $i: DIFFERS"
		cat "$dir/ab"
		fail=1
	fi
	sed 's/^/    /' "$dir/aerr" "$dir/berr"
	i=$((i + 1))
done
exit $fail
