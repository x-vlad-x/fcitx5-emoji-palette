%global upstream_version 0.1.0
%global prerelease_version rc.2
%global base_release 10

Name:           fcitx5-emoji-palette
Version:        0.1.0~rc2
Release:        %{base_release}%{?dist}
Summary:        Native emoji picker for Fcitx 5

License:        GPL-3.0-or-later AND Unicode-3.0 AND CC0-1.0
URL:            https://github.com/x-vlad-x/fcitx5-emoji-palette
Source0:        %{url}/archive/refs/tags/v%{upstream_version}-%{prerelease_version}.tar.gz

BuildRequires:  cmake >= 3.27
BuildRequires:  desktop-file-utils
BuildRequires:  fcitx5-devel >= 5.1
BuildRequires:  gcc-c++
BuildRequires:  libappstream-glib
BuildRequires:  libxkbcommon-devel >= 1.0
BuildRequires:  ninja-build
BuildRequires:  python3
BuildRequires:  qt6-linguist
BuildRequires:  qt6-qtbase-devel >= 6.6
BuildRequires:  qt6-qttools-devel
BuildRequires:  layer-shell-qt-devel >= 6.6
BuildRequires:  reuse

Requires:       fcitx5 >= 5.1
# Fedora publishes no generic font(:lang=und-zsye) provide, so the concrete
# emoji font package is named. Without it every catalog entry would fall back
# to a placeholder tile.
Requires:       google-noto-color-emoji-fonts

%description
Emoji Palette for Fcitx 5 is a searchable, keyboard-driven emoji picker. It
commits the selected text through the original Fcitx input context without
using the clipboard or synthetic input. The helper uses LayerShellQt on
Wayland and supports English, German, and Russian search data.

%prep
%autosetup -n %{name}-%{upstream_version}-%{prerelease_version}
reuse lint

%build
%cmake -G Ninja \
    -DEMOJI_PALETTE_WARNINGS_AS_ERRORS=ON \
    -DEMOJI_PALETTE_BUILD_FCITX_ADDON=ON \
    -DEMOJI_PALETTE_BUILD_UI=ON
%cmake_build

%install
%cmake_install

%check
%ctest
desktop-file-validate \
    %{buildroot}%{_datadir}/applications/org.fcitx.Fcitx5.EmojiPalette.desktop
appstream-util validate-relax --nonet \
    %{buildroot}%{_metainfodir}/org.fcitx.Fcitx5.EmojiPalette.metainfo.xml

%files
%license LICENSE LICENSES/CC0-1.0.txt LICENSES/GPL-3.0-or-later.txt
%license LICENSES/Unicode-3.0.txt
%doc CHANGELOG.md README.md THIRD_PARTY_NOTICES.md
%{_libdir}/fcitx5/libemoji-palette.so
%{_libexecdir}/fcitx5-emoji-palette-ui
%{_datadir}/applications/org.fcitx.Fcitx5.EmojiPalette.desktop
%{_datadir}/dbus-1/services/org.fcitx.Fcitx5.EmojiPalette1.service
%{_datadir}/fcitx5/addon/emojipalette.conf
%{_datadir}/fcitx5-emoji-palette/
%{_metainfodir}/org.fcitx.Fcitx5.EmojiPalette.metainfo.xml

%changelog
* Sun Aug 02 2026 Vladislav Shadiuk <x-vlad-x@users.noreply.github.com> - 0.1.0~rc2-10
- Place the picker next to the text caret and center it on the active
  output when the frontend reports no cursor position

* Thu Jul 30 2026 Vladislav Shadiuk <x-vlad-x@users.noreply.github.com> - 0.1.0~rc2-6
- Consume every key event while the picker is active and translate printable
  search keys on any keyboard layout

* Wed Jul 29 2026 Vladislav Shadiuk <x-vlad-x@users.noreply.github.com> - 0.1.0~rc2-5
- Resolve the packaging test target from the spec instead of a fixed release

* Wed Jul 29 2026 Vladislav Shadiuk <x-vlad-x@users.noreply.github.com> - 0.1.0~rc2-4
- Draw labelled placeholders for emoji the installed font cannot render

* Tue Jul 28 2026 Vladislav Shadiuk <x-vlad-x@users.noreply.github.com> - 0.1.0~rc2-3
- Keep the picker active while using inline grid settings

* Tue Jul 28 2026 Vladislav Shadiuk <x-vlad-x@users.noreply.github.com> - 0.1.0~rc2-2
- Prevent addon startup before Fcitx input-method groups are initialized

* Mon Jul 27 2026 Vladislav Shadiuk <x-vlad-x@users.noreply.github.com> - 0.1.0~rc2-1
- Publish release-asset-safe filenames

* Mon Jul 27 2026 Vladislav Shadiuk <x-vlad-x@users.noreply.github.com> - 0.1.0~rc1-1
- Initial prerelease
