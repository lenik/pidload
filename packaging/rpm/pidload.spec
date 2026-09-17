# Version is injected by packaging/rpm/Makefile via `zfr version`.
# RPM Version cannot contain '-'; use `zfr version -r` (hyphens → '_').
# srcversion is the unsanitized Meson/git version and names the tarball.
%{!?version:%global version 0.0.0}
%{!?srcversion:%global srcversion %{version}}

Name:           pidload
Version:        %{version}
Release:        1%{?dist}
Summary:        wxWidgets process and traffic monitor

License:        AGPL-3.0-or-later
URL:            https://github.com/lenik/pidload
Packager:       Lenik <pidload@bodz.net>
Source0:        %{name}-%{srcversion}.tar.xz

BuildRequires:  meson
BuildRequires:  ninja-build
BuildRequires:  pkgconf
BuildRequires:  asciidoctor
BuildRequires:  libbas-c-devel
BuildRequires:  wxGTK3-devel
BuildRequires:  libX11-devel
BuildRequires:  openssl-devel

%description
pidload charts process CPU/I/O and selected block-device and network
traffic in a sliding window, with optional per-interval log recording.

%prep
%setup -q -n %{name}-%{srcversion}

%build
meson setup build \
    --prefix=%{_prefix} \
    --bindir=%{_bindir} \
    --datadir=%{_datadir} \
    --mandir=%{_mandir} \
    --sysconfdir=%{_sysconfdir} \
    --localstatedir=%{_localstatedir} \
    --buildtype=plain
meson compile -C build

%install
meson install -C build --destdir=%{buildroot}

%files
%{_bindir}/pidload
%{_datadir}/bash-completion/completions/pidload
%{_mandir}/man1/pidload.1*
%{_mandir}/*/man1/pidload.1*
%{_datadir}/locale/*/LC_MESSAGES/pidload.mo
%{_datadir}/doc/%{name}/

%changelog
* Thu Aug 20 2026 Lenik <pidload@bodz.net>
- Align spec with debian/control (Meson, AGPL-3.0-or-later).
- Version comes from `zfr version`, the same method meson.build uses.
