# -*- mode: ruby -*-
# vi: set ft=ruby :

require 'socket'

# Vagrantfile API/syntax version. Don't touch unless you know what you're doing!
VAGRANTFILE_API_VERSION = "2"

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|
  # specify memory size in MiB
  vm_ram = ENV['VAGRANT_VM_RAM'] || 2048
  vm_cpu = ENV['VAGRANT_VM_CPU'] || 2
  vm_ram_bytes = vm_ram * 1024 * 1024

  config.vm.hostname = "gdal-vagrant"
  config.vm.host_name = "gdal-vagrant"

  # proxy configurations.
  # these options are also specified by environment variables;
  #   VAGRANT_HTTP_PROXY, VAGRANT_HTTPS_PROXY, VAGRANT_FTP_PROXY
  #   VAGRANT_NO_PROXY, VAGRANT_SVN_PROXY, VAGRANT_GIT_PROXY
  # if you want to set these on Vagrantfile, edit the following:
  if Vagrant.has_plugin?("vagrant-proxyconf")
    config.proxy.enabled   = false  # true|false
    #config.proxy.http      = "http://192.168.0.2:3128"
    #config.proxy.ftp       = "http://192.168.0.2:3128"
    #config.proxy.https     = "DIRECT"
    #config.proxy.no_proxy  = "localhost,127.0.0.1,.example.com"
    #config.svn_proxy.http  = ""
    #config.git_proxy.http  = ""
  end

  config.vm.provider :virtualbox do |vb,ovrd|
     ovrd.vm.network :forwarded_port, guest: 80, host: 8080
     ovrd.vm.box = "ubuntu/trusty64"
     vb.customize ["modifyvm", :id, "--memory", vm_ram]
     vb.customize ["modifyvm", :id, "--cpus", vm_cpu]
     vb.customize ["modifyvm", :id, "--ioapic", "on"]
     vb.name = "gdal-vagrant"
  end

  config.vm.provider :lxc do |lxc,ovrd|
    ovrd.vm.box = "cultuurnet/ubuntu-14.04-64-puppet"
    lxc.backingstore = 'dir'
    lxc.customize 'cgroup.memory.limit_in_bytes', vm_ram_bytes
    # LXC 3 or later deprecated old parameter
    lxc.customize 'apparmor.allow_incomplete', 1
    # for LXC 2.1 or before
    #lxc.customize 'aa_allow_incomplete', 1
    lxc.container_name = "gdal-vagrant"
    # allow android adb connection from guest
    #ovrd.vm.synced_folder('/dev/bus', '/dev/bus')
    # allow runnng wine inside lxc
    ovrd.vm.synced_folder('/tmp/.X11-unix/', '/tmp/.X11-unix/')
  end

  config.vm.provider :hyperv do |hyperv,ovrd|
    ovrd.vm.box = "withinboredom/Trusty64"
    ovrd.ssh.username = "vagrant"
    hyperv.cpus = vm_cpu
    hyperv.memory = vm_ram
    # If you want to avoid copying an entire image with
    # differencing disk feature, uncomment a following line.
    # hyperv.differencing_disk = true
    hyperv.vmname = "gdal-vagrant"
  end

  ppaRepos = [
    "ppa:openjdk-r/ppa",
    "ppa:ubuntugis/ubuntugis-unstable",
    "ppa:miurahr/gdal-dev-additions"
  ]

  packageList = [
    "autoconf",
    "automake",
    "libtool",
    "subversion",
    "python-numpy",
    "python-dev",
    "python-lxml",
    "postgis",
    "postgresql-server-dev-9.3",
    "postgresql-9.3-postgis-2.2",
    "postgresql-9.3-postgis-scripts",
    "libmysqlclient-dev",
    "libpq-dev",
    "libpng12-dev",
    "libjpeg-dev",
    "libgif-dev",
    "liblzma-dev",
    "libgeos-dev",
    "libcurl4-gnutls-dev",
    # "libproj-dev",
    "libxml2-dev",
    "libexpat-dev",
    "libxerces-c-dev",
    "libnetcdf-dev",
    "netcdf-bin",
    "libpoppler-dev",
    "libpoppler-private-dev",
    "gpsbabel",
    "libboost-all-dev",
    "libgmp-dev",
    "libmpfr-dev",
    "libkml-dev",
    "swig",
    "libhdf4-alt-dev", # libhdf4-dev conflicts with netcdf and crashes at runtime
    "libhdf5-dev",
    "poppler-utils",
    "libfreexl-dev",
    "unixodbc-dev",
    "libwebp-dev",
    "openjdk-8-jdk",
    "libepsilon-dev",
    "libgta-dev",
    "liblcms2-2",
    "libjasper-dev",
    "libarmadillo-dev",
    "libcrypto++-dev",
    "libdap-dev",
    "libogdi3.2-dev",
    "libcfitsio3-dev",
    "libfyba-dev",
    "libsfcgal-dev",
    "couchdb",
    "libmongo-client-dev",
    "libqhull-dev",
    "make",
    "g++",
    "bison",
    "flex",
    "doxygen",
    "texlive-latex-base",
    "vim",
    "ant",
    "unzip",
    "mono-devel",
    "libmono-system-drawing4.0-cil",
    "libjson-c-dev",
    "libtiff5-dev",
    "libopenjp2-7-dev",
    "libopenjpip7",
    "libopenjp3d7",
    "clang-3.9",
    "cmake3",
    "git",
    "wine",
    "ccache",
    "curl",
    "mingw-w64",
    "mingw-w64-i686-dev",
    "mingw-w64-x86-64-dev",
    "mingw-w64-tools",
    "gdb-mingw-w64-target",
    "libgeos-mingw-w64-dev",
    # "libproj-mingw-w64-dev",
    "cmake3-curses-gui",
    "gdb",
    "gdbserver",
    "ninja-build",
    "openjdk-8-jdk",
    "ghostscript",
#    "grass-dev",
    "libcharls-dev",
    "libgeotiff-dev",
    "libgeotiff-epsg",
    "sqlite3",
    "sqlite3-pcre",
    "libpcre3-dev",
    "libspatialite-dev",
    "librasterlite2-dev",
    "libkea-dev",
    "libzstd-dev"
  ];

  if Vagrant.has_plugin?("vagrant-cachier")
    config.cache.scope = :box
    config.cache.enable :generic, {
        "wget" => { cache_dir: "/var/cache/wget" },
      }
  end

  if Dir.glob("#{File.dirname(__FILE__)}/.vagrant/machines/default/*/id").empty?
	  pkg_cmd = "sed -i 's#deb http[s]?://.*archive.ubuntu.com/ubuntu/#deb mirror://mirrors.ubuntu.com/mirrors.txt#' /etc/apt/sources.list; "
      pkg_cmd << 'echo "deb mirror://mirrors.ubuntu.com/mirrors.txt trusty universe" > /etc/apt/sources.list.d/official-ubuntu-trusty-universe.list; '
      pkg_cmd << 'echo "deb [ arch=amd64 ] http://repo.mongodb.org/apt/ubuntu trusty/mongodb-org/3.4 multiverse" > /etc/apt/sources.list.d/mongodb-org-3.4.list; '
      pkg_cmd << 'curl -Ls https://www.mongodb.org/static/pgp/server-3.4.asc | apt-key add -; '
      pkg_cmd << "apt-get update -qq; apt-get install -q -y python-software-properties; "
      pkg_cmd << "apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys 3FA7E0328081BFF6A14DA29AA6A19B38D3D831EF ; "
      pkg_cmd << 'echo "deb https://download.mono-project.com/repo/ubuntu stable-trusty main" > /etc/apt/sources.list.d/mono-official-stable.list; '
      pkg_cmd << "dpkg --add-architecture i386; "

	  if ppaRepos.length > 0
		  ppaRepos.each { |repo| pkg_cmd << "add-apt-repository -y " << repo << " ; " }
		  pkg_cmd << "apt-get update -qq; "
	  end

	  # install packages we need
	  pkg_cmd << "apt-get --no-install-recommends install -q -y " + packageList.join(" ") << " ; "
	  config.vm.provision :shell, :inline => pkg_cmd
    scripts = [
      "install-proj6.sh",
      "gdal.sh",
      "postgis.sh",
      "install-proj6-mingw.sh",
      "gdal-mingw.sh"
    ];
    scripts.each { |script| config.vm.provision :shell, :privileged => false, :path => "gdal/scripts/vagrant/" << script }
  end
end
