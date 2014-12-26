# -*- mode: ruby -*-
# vi: set ft=ruby :

require 'socket'

# Vagrantfile API/syntax version. Don't touch unless you know what you're doing!
VAGRANTFILE_API_VERSION = "2"

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|

  vm_ram = ENV['VAGRANT_VM_RAM'] || 1024
  vm_cpu = ENV['VAGRANT_VM_CPU'] || 2

  config.vm.box = "precise64"

  config.vm.hostname = "gdal-vagrant"
  config.vm.box_url = "http://files.vagrantup.com/precise64.box"
  config.vm.host_name = "gdal-vagrant"
  
  config.vm.network :forwarded_port, guest: 80, host: 8080

  config.vm.provider :virtualbox do |vb|
     vb.customize ["modifyvm", :id, "--memory", vm_ram]
     vb.customize ["modifyvm", :id, "--cpus", vm_cpu]
     vb.customize ["modifyvm", :id, "--ioapic", "on"]
     vb.name = "gdal-vagrant"
   end  

  ppaRepos = [
    "ppa:ubuntugis/ubuntugis-unstable", "ppa:marlam/gta"
  ]

  packageList = [
    "subversion",
    "python-numpy",
    "python-dev",
    "postgis",
    "postgresql-server-dev-9.1",
    "postgresql-9.1-postgis",
    "postgresql-9.1-postgis-scripts",
    "libmysqlclient-dev",
    #"mysql-server",
    "libpq-dev",
    "libpng12-dev",
    "libjpeg-dev",
    "libgif-dev",
    "liblzma-dev",
    "libgeos-dev",
    "libcurl4-gnutls-dev",
    "libproj-dev",
    "libxml2-dev",
    "libexpat-dev",
    "libxerces-c-dev",
    "libnetcdf-dev",
    "netcdf-bin",
    "libpoppler-dev",
    "libspatialite-dev",
    "gpsbabel",
    "swig",
    "libhdf4-alt-dev",
    "libhdf5-serial-dev",
    "libpodofo-dev",
    "poppler-utils",
    "libfreexl-dev",
    "unixodbc-dev",
    "libwebp-dev",
    "openjdk-7-jdk",
    "libepsilon-dev",
    "libgta-dev",
    "liblcms2-2",
    "libpcre3-dev",
    "libjasper-dev",
    "libarmadillo-dev",
    "make",
    "g++",
    "autoconf", # for libkml
    "cmake", # for openjpeg
    "bison",
    "flex",
    "doxygen",
    "vim"
  ];

  if Dir.glob("#{File.dirname(__FILE__)}/.vagrant/machines/default/*/id").empty?
	  pkg_cmd = "sed -i 's#deb http://us.archive.ubuntu.com/ubuntu/#deb mirror://mirrors.ubuntu.com/mirrors.txt#' /etc/apt/sources.list; "

	  pkg_cmd << "apt-get update -qq; apt-get install -q -y python-software-properties; "

	  if ppaRepos.length > 0
		  ppaRepos.each { |repo| pkg_cmd << "add-apt-repository -y " << repo << " ; " }
		  pkg_cmd << "apt-get update -qq; "
	  end

	  # install packages we need we need
	  pkg_cmd << "apt-get install -q -y " + packageList.join(" ") << " ; "
	  config.vm.provision :shell, :inline => pkg_cmd
    scripts = [
      "libkml.sh",
      "openjpeg.sh",
      "gdal.sh",
      "postgis.sh"
    ];
    scripts.each { |script| config.vm.provision :shell, :privileged => false, :path => "scripts/vagrant/" << script }
  end
end
