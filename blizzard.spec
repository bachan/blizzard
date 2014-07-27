Summary: 	Blizzard http server
Name: 		blizzard
Version: 	0.3.4
Release: 	0%{?dist}
License: 	BSD
Source: 	blizzard-%{version}.tar.gz
Group:		Networking/Daemons
BuildRoot: 	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires:	libev-devel cmake gcc-c++

%package devel
Summary:	Header files and development documentation for %{name}
Group: 		Development/Libraries
Requires:	%{name} = %{version}-%{release}

%description
Blizzard http server.
%description devel
Blizzard header files.

This package contains the header files, static libraries and development
documentation for %{name}. If you like to develop modules for %{name},
you will need to install %{name}-devel.

%prep
%setup -q -n blizzard-%{version}

%build
cmake -D SKIP_RELINK_RPATH=ON . -DCMAKE_INSTALL_PREFIX=/usr -DCFLAGS="${CFLAGS}" -DCXXFLAGS="${CXXFLAGS}"
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
mkdir %{buildroot}
make DESTDIR=%{buildroot} install

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%{_bindir}/blizzard
%{_prefix}/etc/blizzard/config.xml
%{_prefix}/etc/blzmod_example/config.xml
%{_prefix}/etc/blzmod_example/config_module.xml
%{_libdir}/libblzmod_example.so

%files devel
%defattr(-,root,root,-)
%{_includedir}/blizzard

%changelog
* Sun Jul 27 2014 Alexander Pankov <pianist@usrsrc.ru> - 0.3.4-0
+ First RPM build

