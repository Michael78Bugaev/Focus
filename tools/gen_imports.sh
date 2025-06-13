#!/usr/bin/env bash
# Generate NASM assembly file with absolute addresses of kernel symbols
# Usage: tools/gen_imports.sh <kernel-elf> <output.S>
set -e
if [ $# -lt 2 ]; then
  echo "Usage: $0 build/kernel output.S" >&2
  exit 1
fi
KERNELELF="$1"
OUT="$2"
# List of symbols to export. Edit as needed.
SYMS=(kprintf malloc mfree strcmp strcpy strlen memcpy memset)
{
  echo '; Auto-generated – do not edit'
  for S in "${SYMS[@]}"; do
    ADDR=$(nm -n "$KERNELELF" | awk -v sym="$S" '$3==sym {print $1}')
    if [ -z "$ADDR" ]; then
      echo "; Warning: symbol $S not found in kernel" >&2
      continue
    fi
    echo "global $S"
    echo "$S equ 0x$ADDR"
  done
} > "$OUT" 