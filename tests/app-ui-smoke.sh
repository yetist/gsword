#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com>
#
# app-ui-smoke.sh: launch GSword headlessly and verify it starts and stays
# alive without application-level crashes.  GTK critical checks belong to the
# reference-chooser ui-smoke test; this script intentionally ignores
# environment-specific GTK warnings from headless accessibility/GPU setup.

set -eu

app=$1
tmp=$(mktemp -d "${TMPDIR:-/tmp}/gsword-ui-smoke.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

mkdir -p "$tmp/config" "$tmp/data" "$tmp/cache" "$tmp/library"
schema_dir=$(dirname "$app")

export XDG_CONFIG_HOME="$tmp/config"
export XDG_DATA_HOME="$tmp/data"
export XDG_CACHE_HOME="$tmp/cache"
export GSETTINGS_BACKEND=memory
export GSETTINGS_SCHEMA_DIR="$schema_dir"
export GSWORD_LIBRARY="$tmp/library"
export GTK_A11Y=none
export NO_AT_BRIDGE=1
export GDK_BACKEND=x11
export GSK_RENDERER=cairo

set +e
timeout 8 "$app" >"$tmp/out" 2>"$tmp/err"
status=$?
set -e

if [ -s "$tmp/out" ]; then
    cat "$tmp/out" >&2
fi
if [ -s "$tmp/err" ]; then
    cat "$tmp/err" >&2
fi

if [ "$status" -ne 0 ] && [ "$status" -ne 124 ]; then
    echo "app-ui-smoke: application exited with status $status" >&2
    exit 1
fi

if grep -Eq 'Segmentation fault|core dumped|Aborted|Trace/breakpoint trap' \
        "$tmp/err"; then
    echo "app-ui-smoke: application crashed" >&2
    exit 1
fi

exit 0
