%define ls_fuse_version 0.3

Name:		ls-fuse
Version:	%{ls_fuse_version}
Release:	1%{?dist}
Summary:	mount output of ls utility

Group:		Application/System
License:	GPL-3+
URL:		https://sourceforge.net/projects/lsfuse
Source0:	ls-fuse-%{ls_fuse_version}.tar.bz2

Requires:	fuse

%description
ls-fuse mounts output of 'ls -lR', 'ls -lRZ' or 'ls -l' as a pseudo filesystem.
Purpose of ls-fuse project is similar to lsfs project or lslR plugin for midnight commander. But the main goal was implementation of tool with SELinux extended attributes support.

%prep
%setup -n ls-fuse-%{ls_fuse_version}

%build
%configure
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot}

%files
%{_bindir}/ls-fuse
%{_mandir}/man1/ls-fuse.1.gz

%changelog
