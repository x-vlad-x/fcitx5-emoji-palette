# Contributing

Development follows a trunk-based workflow. Create a short-lived branch from
`main`, keep the change focused, and open a pull request.

Use Conventional Commit style for commit messages and pull request titles.
Comments in source code should be reserved for invariants or constraints that
cannot be expressed clearly by names, types, or structure. All source comments,
documentation, issue text, pull request text, and commit messages must be in
English.

## Development environment

Fedora 44 provides the reference toolchain:

```bash
sudo dnf install cmake ninja-build gcc-c++ clang clang-tools-extra python3 \
  fcitx5-devel qt6-qtbase-devel qt6-linguist qt6-qttools-devel \
  layer-shell-qt-devel reuse
```

An OCI or Distrobox Fedora environment is appropriate for development on an
immutable host. Do not layer build dependencies onto Bazzite solely to compile
the project.

## Required checks

Run the normal build and tests:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Check formatting and licensing:

```bash
find include src tests -type f \( -name '*.cpp' -o -name '*.hpp' \) -print0 |
  xargs -0 clang-format --dry-run --Werror
reuse lint
git diff --check
```

Changes to generated Unicode data must follow `docs/unicode-data.md` and
produce a deterministic result. Packaging changes must build both a binary RPM
and an SRPM and pass `rpmlint`, desktop-file validation, and AppStream
validation.

Before requesting review, record the exact commands and results in the pull
request description. UI or platform changes should update
`docs/manual-test-matrix.md` only when evidence was collected on the stated
target environment.

Do not add copied code, data, translations, or artwork without verified license
compatibility, SPDX metadata, and an update to `THIRD_PARTY_NOTICES.md`.

Security vulnerabilities must be reported privately as described in
`SECURITY.md`.
