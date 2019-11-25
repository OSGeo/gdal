Name:           proj6
Version:        6.2.1
%global         gittag %{version}
%global         prefix_path /usr/local/proj6
Release:        1%{?dist}
Summary:        OSGEO PROJ coordinate transformation software library.
License:        GPL
URL:            https://github.com/OSGeo/PROJ.git
#Source0:        https://github.com/OSGeo/PROJ/releases/download/%{gittag}/proj-%{version}.tar.gz
Source0:        proj-%{version}.tar.gz

BuildRequires:  sqlite-devel chrpath
Requires:       sqlite


%description
PROJ contributors (2019). PROJ coordinate transformation
software library. Open Source Geospatial Foundation. 
URL https://proj.org/.

%prep
%setup -q -n proj-%{version}

%build
./configure --prefix=%{prefix_path}
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
%make_install

chrpath --delete $RPM_BUILD_ROOT%{prefix_path}/bin/proj \
                 $RPM_BUILD_ROOT%{prefix_path}/bin/geod \
                 $RPM_BUILD_ROOT%{prefix_path}/bin/cs2cs \
                 $RPM_BUILD_ROOT%{prefix_path}/bin/gie \
                 $RPM_BUILD_ROOT%{prefix_path}/bin/cct \
                 $RPM_BUILD_ROOT%{prefix_path}/bin/projinfo

%files
%{prefix_path}/bin/*
%{prefix_path}/lib/*
%{prefix_path}/share/*
%{prefix_path}/include/*

%doc

%changelog
