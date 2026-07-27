# Packaging and installation

The supported package output for the first prerelease is a Fedora 44 binary
RPM plus its source RPM. Both artifacts are built from the same source archive
and spec file.

## Fedora

Install a downloaded release artifact:

```bash
sudo dnf install ./fcitx5-emoji-palette-0.1.0-rc.2-1.fc44.x86_64.rpm
fcitx5 -r
```

Remove it:

```bash
sudo dnf remove fcitx5-emoji-palette
fcitx5 -r
```

The package installs the Fcitx addon, crash-isolated helper, D-Bus activation
service, desktop metadata, AppStream metadata, translations, and Noto Color
Emoji runtime dependency.

## Bazzite

Fcitx loads addons from the host deployment. A Flatpak, Homebrew package, or
Distrobox export cannot insert this shared library into the running host Fcitx
process. Package layering is therefore the direct installation path.

Download the RPM matching the Fedora base and machine architecture, then:

```bash
sudo rpm-ostree install ./fcitx5-emoji-palette-0.1.0-rc.2-1.fc44.x86_64.rpm
systemctl reboot
```

After booting the new deployment, verify the package and addon:

```bash
rpm -q fcitx5-emoji-palette
fcitx5-diagnose
```

Local RPM files do not receive automatic package updates. For each new
project release, download the replacement RPM and run:

```bash
sudo rpm-ostree uninstall fcitx5-emoji-palette
sudo rpm-ostree install ./fcitx5-emoji-palette-NEW_VERSION.fc44.x86_64.rpm
systemctl reboot
```

Remove the package:

```bash
sudo rpm-ostree uninstall fcitx5-emoji-palette
systemctl reboot
```

Use `rpm-ostree status` before and after each operation. If a layered dependency
conflict blocks an operating-system upgrade, remove this package, reboot,
complete the Bazzite update, and install a release built for the new Fedora
base. `rpm-ostree rollback` can return to the previous deployment if the newly
booted deployment fails.

Bazzite documents package layering as a last-resort system-level mechanism
because incompatible layered packages can block updates or rebases. This
project requires it only because the Fcitx addon must be installed on the host.
See the current
[Bazzite package-layering documentation](https://docs.bazzite.gg/Installing_and_Managing_Software/rpm-ostree/)
before installing.

## Custom Bazzite image

For a managed fleet or a long-lived personal image, include the RPM during an
image build instead of layering it after deployment. Start from the current
[Universal Blue image template](https://github.com/ublue-os/image-template)
and integrate the steps in `packaging/bazzite/`.

The example is intentionally not a complete image repository. Pin the Bazzite
base digest, verify the RPM checksum or signature, build with the template's
workflow, and test the resulting bootable image before rebasing a machine.

## Build packages locally

Install the packaging dependencies in Fedora 44:

```bash
sudo dnf install cmake desktop-file-utils fcitx5-devel gcc-c++ git \
  google-noto-color-emoji-fonts libappstream-glib libxkbcommon-devel \
  layer-shell-qt-devel \
  ninja-build python3 qt6-linguist qt6-qtbase-devel qt6-qttools-devel \
  reuse rpm-build rpmlint
```

From a clean checkout:

```bash
version=0.1.0-rc.2
mkdir -p rpmbuild/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
git archive \
  --format=tar.gz \
  --prefix="fcitx5-emoji-palette-${version}/" \
  --output="rpmbuild/SOURCES/v${version}.tar.gz" \
  HEAD
cp packaging/fedora/fcitx5-emoji-palette.spec rpmbuild/SPECS/
rpmbuild -ba \
  --define "_topdir $PWD/rpmbuild" \
  rpmbuild/SPECS/fcitx5-emoji-palette.spec
rpmlint \
  rpmbuild/SPECS/fcitx5-emoji-palette.spec \
  rpmbuild/SRPMS/*.src.rpm \
  rpmbuild/RPMS/*/*.rpm
```

Do not install an RPM produced from an untrusted checkout. The binary RPM and
SRPM appear below `rpmbuild/RPMS/` and `rpmbuild/SRPMS/`.
