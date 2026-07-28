#!/usr/bin/bash
# SPDX-FileCopyrightText: 2026 Vladislav Shadiuk <x-vlad-x@users.noreply.github.com>
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail
export LC_ALL=C.UTF-8

expect_addon=true
if [[ ${1:-} == "--fcitx-only" ]]; then
    expect_addon=false
elif [[ $# -ne 0 ]]; then
    echo "Usage: $0 [--fcitx-only]" >&2
    exit 2
fi

addon_library=/usr/lib64/fcitx5/libemoji-palette.so
addon_metadata=/usr/share/fcitx5/addon/emojipalette.conf
helper=/usr/libexec/fcitx5-emoji-palette-ui
service=/usr/share/dbus-1/services/org.fcitx.Fcitx5.EmojiPalette1.service

if [[ "$expect_addon" == true ]]; then
    test -f "$addon_library"
    test -f "$addon_metadata"
    test -x "$helper"
    test -f "$service"
    grep -Fxq 'Library=libemoji-palette' "$addon_metadata"
    grep -Fxq 'Name=org.fcitx.Fcitx5.EmojiPalette1' "$service"
    grep -Fxq "Exec=$helper" "$service"
    if ldd "$addon_library" | grep -F 'not found' >/dev/null; then
        ldd "$addon_library" >&2
        exit 1
    fi
    readelf -d "$addon_library" |
        grep -F 'Shared library: [libxkbcommon.so.0]' >/dev/null
fi

EXPECT_ADDON="$expect_addon" QT_QPA_PLATFORM=minimal dbus-run-session -- bash -euo pipefail -c '
    config_root=$(mktemp -d)
    state_root=$(mktemp -d)
    cache_root=$(mktemp -d)
    log_file=$(mktemp)
    export XDG_CONFIG_HOME="$config_root"
    export XDG_STATE_HOME="$state_root"
    export XDG_CACHE_HOME="$cache_root"
    enabled_addons=keyboard,dbus
    if [[ "$EXPECT_ADDON" == true ]]; then
        enabled_addons+=,emojipalette
    fi
    fcitx5 -D --disable=all --enable="$enabled_addons" \
        --verbose "*"=5 >"$log_file" 2>&1 &
    fcitx_pid=$!

    cleanup() {
        if kill -0 "$fcitx_pid" 2>/dev/null; then
            kill "$fcitx_pid"
            wait "$fcitx_pid" || true
        fi
    }
    trap cleanup EXIT

    ready=false
    for _ in {1..100}; do
        if ! kill -0 "$fcitx_pid" 2>/dev/null; then
            wait "$fcitx_pid" || true
            cat "$log_file" >&2
            exit 1
        fi
        if fcitx5-remote --check >/dev/null 2>&1; then
            ready=true
            break
        fi
        sleep 0.05
    done
    if [[ "$ready" != true ]]; then
        cat "$log_file" >&2
        exit 1
    fi

    process_state=$(ps -o stat= -p "$fcitx_pid")
    [[ "$process_state" != *Z* ]]
    if [[ "$EXPECT_ADDON" != true ]]; then
        exit 0
    fi

    ! grep -Eq "Failed to load addon: emojipalette|Failed to load library.*emoji" "$log_file"

    gdbus introspect \
        --session \
        --dest org.fcitx.Fcitx5.EmojiPalette1 \
        --object-path /org/fcitx/Fcitx5/EmojiPalette1 >"$state_root/introspection.txt"
    grep -Fq 'Exchange' "$state_root/introspection.txt"
    grep -Fq 'Frame' "$state_root/introspection.txt"

    helper_pid=$(pgrep -f "^/usr/libexec/fcitx5-emoji-palette-ui$")
    kill "$helper_pid"
    wait "$helper_pid" 2>/dev/null || true
    fcitx5-remote --check >/dev/null
'
