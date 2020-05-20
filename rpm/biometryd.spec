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
Requires:       pkgconfig
Requires:       libdbus-cpp5

%description
Biometry daemon


%prep
%setup -q -n %{name}-%{version}

%build
%cmake . -DBIOMETRYD_VERSION_MAJOR=0 -DBIOMETRYD_VERSION_MINOR=0 -DBIOMETRYD_VERSION_PATCH=1
make

%install
%make_install

%files
%defattr(-,root,root,-)
%{_sysconfdir}/init/biometryd.conf
%{_sysconfdir}/dbus-1/system.d/com.ubuntu.biometryd.Service.conf
%{_bindir}/%{name}
%{_libdir}/pkgconfig/biometryd.pc
%{_libdir}/libbiometry.so*
%{_libdir}/qt5/qml/Biometryd/*
%{_includedir}/biometry/*

