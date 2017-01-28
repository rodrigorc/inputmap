#!/bin/bash
set -e

LEMON="$1"
shift
BUILDDIR="$1"
shift
SRC="$1"
shift

SRCBASE="${SRC%.*}"
"$LEMON" "$SRC" "$@"
mv "$SRCBASE."{h,out} "$BUILDDIR"
mv "$SRCBASE.c" "$BUILDDIR"/"$(basename "$SRCBASE").cpp"
