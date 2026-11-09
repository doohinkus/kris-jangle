#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# The shared FM-1 builder calls this (zvenson/fm1-build): ./build.sh --release X.Y
# builds kris-jangle; with no arguments it does a dev build.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
exec python3 "$here/tools/build.py" "$here/kris-jangle" "$@"
