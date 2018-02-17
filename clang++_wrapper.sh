#!/bin/bash

ARGS=()
for var in "$@"; do
	[ "$var" != '-fno-plt' ] && ARGS+=("$var")
done
/opt/h2oai/dai/bin/clang++ "${ARGS[@]}"

