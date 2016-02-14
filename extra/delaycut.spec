Name:       delaycut
Version:	1.4.3.8
Release:	1%{?dist}
License:	GPLv3+
Summary:	Delaycut corrects delay and is also able to cut audio files coded ac3, dts, mpa and wav, It is also able to fix CRC errors in ac3 and mpa files
Url:    https://github.com/darealshinji/delaycut
Group:	Applications/Multimedia
Source: delaycut.tar.gz
Requires: qt
BuildRequires:	qt-devel
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

%description
DelayCut corrects delay and is also able to cut audio files
coded ac3, dts, mpa and wav.
It is also able to fix CRC errors in ac3 and mpa files.

%prep
%setup -q -n %{name}

%build
qmake-qt5 delaycut.pro
make

%install
install -D -m755 delaycut %{buildroot}/usr/bin/delaycut
install -D -m755 extra/delaycut.desktop %{buildroot}/usr/share/applications

%files
%defattr(-,root,root)
%doc README COPYING ChangeLog
/usr/*

%changelog
* Sun Feb 14 2016 <djcj AT gmx DOT de> 1.4.3.8
- new release version
* Fri Aug 16 2013 <davidjeremias82 AT gmail DOT com> 1.4.3.7-1
- initial rpm for Fedora 
