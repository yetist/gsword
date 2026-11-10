#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: Copyright (c) 2026 Xiaotian Wu <yetist@gmail.com>
#
# app-ui-smoke.sh: launch GSword headlessly and verify it starts without
# application-level crashes or GTK criticals.  The caller normally runs this
# script through xvfb-run.

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

if grep -Eq 'Segmentation fault|Gtk-CRITICAL|GLib-GObject-CRITICAL|ERROR:' \
        "$tmp/err"; then
    echo "app-ui-smoke: application produced critical output" >&2
    exit 1
fi

exit 0
