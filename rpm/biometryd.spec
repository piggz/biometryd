Name:       biometryd

# >> macros
# << macros

Summary:    Biometry Daemon
Version:    0.0.1
Release:    1
Group:      libs
License:    LICENSE
URL:        https://launchpad.net/biometryd
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  cmake
BuildRequires:  dbus-cpp-devel
BuildRequires:  gcc-c++
BuildRequires:  process-cpp-devel
BuildRequires:  sqlite-devel
BuildRequires:  boost-filesystem
BuildRequires:  boost-program-options
BuildRequires:  boost-system
BuildRequires:  boost-devel
BuildRequires:  libhybris-devel
BuildRequires:  elfutils-libelf-devel
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
Requires:       pkgconfig
Requires:       libdbus-cpp5

%description
Biometry daemon

%package devel
Summary: Biometry daemon development package
Requires:  biometryd = %{version}-%{release}

%description devel
%{summary}

%prep
%setup -q -n %{name}-%{version}

%build
%cmake . -DBIOMETRYD_VERSION_MAJOR=0 -DBIOMETRYD_VERSION_MINOR=0 -DBIOMETRYD_VERSION_PATCH=1
make

%install
%make_install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_sysconfdir}/init/biometryd.conf
%{_sysconfdir}/dbus-1/system.d/com.ubuntu.biometryd.Service.conf
%{_bindir}/%{name}
%{_libdir}/libbiometry.so*
%{_libdir}/qt5/qml/Biometryd/*

%files devel
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/biometryd.pc
%{_includedir}/biometry/*
