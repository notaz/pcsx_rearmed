#!/bin/bash
set -e

url_base="http://ports.ubuntu.com/ubuntu-ports/"
paths=`apt-cache show "$@" | grep '^Filename:' | awk '{print $2}'`
for p in $paths; do
  base=`basename $p`
  wget -nv "${url_base}${p}"
  echo "exctacting $base"
  dpkg-deb -x "$base" .
done
