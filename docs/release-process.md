# Release process

## Prepare

1. Create a release issue and branch from the current protected `main`.
2. Choose a version and update `CMakeLists.txt`, the Fedora spec, AppStream
   metadata, and `CHANGELOG.md`.
3. Confirm that Unicode and CLDR versions remain pinned and supported.
4. Complete all automated checks and review their full logs.
5. Execute the manual test matrix on the supported desktop. A prerelease may
   retain explicit `Not run` rows; a stable release may not.

## Build

Create the source archive from the exact release commit and build both RPM
artifacts with `rpmbuild -ba`. Run:

- GCC and Clang debug and release builds;
- all CTest targets;
- AddressSanitizer and UndefinedBehaviorSanitizer;
- clang-format, clang-tidy, and CodeQL;
- REUSE, desktop-file, AppStream, and rpmlint validation;
- packaged installation and addon load smoke tests in Fedora 44;
- deterministic Unicode generator verification.

Inspect the binary RPM file list and dependencies. Install it only in an
ephemeral Fedora environment until a host test is explicitly authorized.

## Publish

1. Merge the release pull request with all required checks green.
2. Create a signed annotated `vVERSION` tag on the merge commit.
3. Create a GitHub prerelease or release from that tag.
4. Attach the binary RPM and SRPM produced from the tagged source.
5. Publish checksums and concise release notes with known limitations.
6. Verify that the public artifacts install in a fresh Fedora 44 environment.

## After publication

Keep unreleased changes under `Unreleased` in the changelog. Monitor security
reports privately and normal defects in the issue tracker. If an artifact is
incorrect, publish a new release; do not silently replace immutable release
assets.
