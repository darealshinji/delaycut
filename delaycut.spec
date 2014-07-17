Name:       delaycut
Version:	1.4.3.7
Release:	1%{?dist}
License:	GPLv3+
Summary:	Delaycut corrects delay and is also able to cut audio files coded ac3, dts, mpa and wav, It is also able to fix CRC errors in ac3 and mpa files
Url:    https://github.com/darealshinji/delaycut
#Url:	https://github.com/athomasm/delaycut
Group:	Applications/Multimedia
#Source from: git clone https://github.com/athomasm/delaycut.git
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
qmake-qt4 delaycut.pro
make

%install
install -D -m755 delaycut %{buildroot}/usr/bin/delaycut

%files
%defattr(755, root, root)
%{_bindir}/delaycut

%changelog
* Fri Aug 16 2013 <davidjeremias82 AT gmail DOT com> 1.4.3.7-1
- initial rpm for Fedora 
