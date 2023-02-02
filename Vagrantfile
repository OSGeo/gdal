# -*- mode: ruby -*-
# vi: set ft=ruby :

require 'socket'

# Vagrantfile API/syntax version. Don't touch unless you know what you're doing!
VAGRANTFILE_API_VERSION = "2"

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|
  # specify memory size in MiB
  vm_ram = ENV['VAGRANT_VM_RAM'] || 4096
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

  config.vm.network :forwarded_port, guest: 80, host: 8080
  # See https://bugs.launchpad.net/cloud-images/+bug/1969664
  # Ubuntu 22.04 no longer accepts RSA keys, which causes issues with older Vagrant
  # The below location has a jammy64 image that accepts RSA keys
  config.vm.box_url = "https://people.canonical.com/~jchittum/vagrant-testing/ubuntu-jammy-vagrant-TESTING-20220419.box"
  config.vm.box = "ubuntu/jammy64"

  config.vm.provider :virtualbox do |vb|
     vb.customize ["modifyvm", :id, "--memory", vm_ram]
     vb.customize ["modifyvm", :id, "--cpus", vm_cpu]
     vb.customize ["modifyvm", :id, "--ioapic", "on"]
     vb.name = "gdal-vagrant"
  end

  #config.vm.provider :lxc do |lxc,ovrd|
  #  ovrd.vm.box = "cultuurnet/ubuntu-14.04-64-puppet"
  #  lxc.backingstore = 'dir'
  #  lxc.customize 'cgroup.memory.limit_in_bytes', vm_ram_bytes
  #  # LXC 3 or later deprecated old parameter
  #  lxc.customize 'apparmor.allow_incomplete', 1
  #  # for LXC 2.1 or before
  #  #lxc.customize 'aa_allow_incomplete', 1
  #  lxc.container_name = "gdal-vagrant"
  #  # allow android adb connection from guest
  #  #ovrd.vm.synced_folder('/dev/bus', '/dev/bus')
  #  # allow runnng wine inside lxc
  #  ovrd.vm.synced_folder('/tmp/.X11-unix/', '/tmp/.X11-unix/')
  #end

  #config.vm.provider :hyperv do |hyperv,ovrd|
  #  ovrd.vm.box = "withinboredom/Trusty64"
  #  ovrd.ssh.username = "vagrant"
  #  hyperv.cpus = vm_cpu
  #  hyperv.memory = vm_ram
  #  # If you want to avoid copying an entire image with
  #  # differencing disk feature, uncomment a following line.
  #  # hyperv.differencing_disk = true
  #  hyperv.vmname = "gdal-vagrant"
  #end

  if Vagrant.has_plugin?("vagrant-cachier")
    config.cache.scope = :box
    config.cache.enable :generic, {
        "wget" => { cache_dir: "/var/cache/wget" },
      }
  end

  # Unless explicitly declined, use the VM host's file system to cache
  # .deb files to avoid repeated downloads on each vagrant up
  unless File.exists?("../.no_apt_cache")
    cache_dir = "../apt-cache/#{config.vm.box}"
    FileUtils.mkdir_p(cache_dir) unless Dir.exists?(cache_dir)
    puts "Using local apt cache, #{cache_dir}"
    config.vm.synced_folder cache_dir, "/var/cache/apt/archives"
  end

  ppaRepos = [
  ]

  packageList = [
    "build-essential",
    "ca-certificates",
    "git",
    "make",
    "ninja-build",
    "cmake",
    "ccache",
    "gdb",
    "g++",
    "mold",
    "bison",
    "flex",
    "wget",
    "curl",
    "unzip",
    "libtool",
    "autoconf",
    "automake",
    "zlib1g-dev",
    "libsqlite3-dev",
    "pkg-config",
    "sqlite3",
    "bash-completion",
    "swig",
    "ant",
    "openjdk-11-jdk",
    "mono-mcs",
    "mono-runtime",
    "libmono-system-drawing4.0-cil",
    "python3-dev",
    "python3-numpy",
    "python3-setuptools",
    "python3-pip",
    "postgis",
    "postgresql",
    "postgresql-postgis",
    "gpsbabel",
    "doxygen",
    "libproj-dev",
    "proj-data",
    "libarchive-dev",
    "libcurl4-gnutls-dev",
    "libtiff5-dev",
    "libopenjp2-7-dev",
    "libcairo2-dev",
    "libpng-dev",
    "libjpeg-dev",
    "libgif-dev",
    "liblzma-dev",
    "libgeos-dev",
    "libxml2-dev",
    "libexpat-dev",
    "libxerces-c-dev",
    "libnetcdf-dev",
    "libpoppler-dev",
    "libpoppler-private-dev",
    "libspatialite-dev",
    "librasterlite2-dev",
    "libhdf4-alt-dev",
    "libhdf5-serial-dev",
    "libfreexl-dev",
    "unixodbc-dev",
    "mdbtools-dev",
    "libwebp-dev",
    "liblcms2-2",
    "libpcre3-dev",
    "libcrypto++-dev",
    "libfyba-dev",
    "libkml-dev",
    "libmysqlclient-dev",
    "libogdi-dev",
    "libcfitsio-dev",
    "libzstd-dev",
    "libpq-dev",
    "libssl-dev",
    "libboost-dev",
    "libarmadillo-dev",
    "libopenexr-dev",
    "libheif-dev",
    "libdeflate-dev",
    "libblosc-dev",
    "liblz4-dev",
    "libbz2-dev",
    "libbrotli-dev",
    "libqhull-dev",
    "libjson-c-dev",
    "libtiff5-dev",
  ];

  config.ssh.forward_agent = true
  config.ssh.forward_x11 = true

  if Dir.glob("#{File.dirname(__FILE__)}/.vagrant/machines/default/*/id").empty?
    pkg_cmd = ""
    if ppaRepos.length > 0
      ppaRepos.each { |repo| pkg_cmd << "add-apt-repository -y " << repo << " ; " }
    end

    # install packages we need
    pkg_cmd << "apt-get update -qq; "
    pkg_cmd << "apt-get --no-install-recommends install -q -y " + packageList.join(" ") << " ; "

    # setup environment when we log in
    pkg_cmd << "echo 'CCACHE_DIR=/vagrant/ccache_vagrant' >> /etc/environment; "
    pkg_cmd << "echo 'cd /vagrant/build_vagrant; source /vagrant/scripts/setdevenv.sh' >> /home/vagrant/.bashrc; "

    config.vm.provision :shell, :inline => pkg_cmd
    scripts = [
      "arrow-parquet.sh",
      "postgis.sh",
      "gdal.sh",
    ];
    scripts.each { |script| config.vm.provision :shell, :privileged => false, :path => "scripts/vagrant/" << script }
  end
end
