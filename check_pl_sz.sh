hex=$(readelf -SW ./build/famine/Famine | awk '$3 == ".text" {print $7}')
printf '%d bytes\n' "$((16#$hex))"