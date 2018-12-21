#!/bin/bash

if [ -n "$LLVM7" ]; then
	exec "$LLVM7/bin/llvm-cov" gcov "$@"

elif [ -n "$LLVM6" ]; then
	exec "$LLVM6/bin/llvm-cov" gcov "$@"

elif [ -n "$LLVM5" ]; then
	exec "$LLVM5/bin/llvm-cov" gcov "$@"

elif [ -n "$LLVM4" ]; then
	exec "$LLVM4/bin/llvm-cov" gcov "$@"

elif [ -f "/usr/local/opt/llvm/bin/llvm-cov" ]; then
	exec "/usr/local/opt/llvm/bin/llvm-cov" gcov "$@"

else
	exec gcov "$@"

fi
