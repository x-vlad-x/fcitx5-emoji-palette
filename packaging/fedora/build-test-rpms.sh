#!/usr/bin/bash
# SPDX-FileCopyrightText: 2026 Vladislav Shadiuk <x-vlad-x@users.noreply.github.com>
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

repository_root=$(git rev-parse --show-toplevel)
cd "$repository_root"

if [[ -n $(git status --porcelain) ]]; then
    echo "Refusing to build an RPM from a dirty worktree." >&2
    exit 1
fi

short_sha=$(git rev-parse --short=12 HEAD)
rpm_root="$repository_root/build/rpmbuild-$short_sha"

if [[ -e "$rpm_root" ]]; then
    echo "Refusing to replace existing artifacts at $rpm_root." >&2
    exit 1
fi

mkdir -p "$rpm_root"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
git archive \
    --format=tar.gz \
    --prefix=fcitx5-emoji-palette-0.1.0-rc.2/ \
    --output="$rpm_root/SOURCES/v0.1.0-rc.2.tar.gz" \
    HEAD
cp packaging/fedora/fcitx5-emoji-palette.spec "$rpm_root/SPECS/"

rpmbuild -ba \
    --define "_topdir $rpm_root" \
    "$rpm_root/SPECS/fcitx5-emoji-palette.spec"

printf 'Source commit: %s\nArtifacts: %s\n' "$(git rev-parse HEAD)" "$rpm_root"
