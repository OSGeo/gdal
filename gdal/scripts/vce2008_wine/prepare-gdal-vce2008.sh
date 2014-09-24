#!/bin/sh

if test "$(lsb_release -c -s)" != "precise"; then
    echo "Only works with Ubuntu 12.04 Precise"
    exit 1
fi

# Install various packages
sudo add-apt-repository -y ppa:ubuntu-wine/ppa
sudo apt-get update -qq
# We need wine1.4 to be able to run nmake.exe. wine1.6 or 1.7 does not work
sudo apt-get install -y wine1.4 wget p7zip-full cabextract unzip

# Download and install Visual Studio Express 2008
if test "$(test -f /tmp/VC.iso && stat -c '%s' /tmp/VC.iso)" != "938108928"; then
    wget "http://go.microsoft.com/fwlink/?LinkId=104679" -O /tmp/VC.iso
fi
rm -rf /tmp/vc
mkdir /tmp/vc
7z x /tmp/VC.iso VCExpress/* -o/tmp/vc
cabextract /tmp/vc/VCExpress/Ixpvc.exe -d /tmp/vc/VCExpress
wine msiexec /i /tmp/vc/VCExpress/vs_setup.msi  VSEXTUI=1 ADDLOCAL=ALL REBOOT=ReallySuppress /qn

# Download and install Platform SDK
for i in PSDK-SDK_Core_BLD-common.0.cab PSDK-SDK_Core_BLD-common.1.cab PSDK-SDK_Core_BLD-common.2.cab PSDK-SDK_Core_BLD_X86-common.0.cab PSDK-SDK_Core_BIN-x86.0.cab PSDK-SDK_MDAC_BLD_X86-common.0.cab PSDK-SDK_MDAC_BLD-common.0.cab; do
    wget http://download.microsoft.com/download/a/5/f/a5f0d781-e201-4ab6-8c6a-9bb4efed1e1a/$i -O /tmp/$i
done
mkdir /tmp/psdk
cabextract -d/tmp/psdk /tmp/*.cab
for i in /tmp/psdk/*; do mv $i `echo $i | sed "s/\(.*\)\..*/\1/" | sed "s/\(.*\)_\(.*\)/\1\.\2/"`; done
mkdir ~/.wine/drive_c/psdk
mv /tmp/psdk ~/.wine/drive_c

# Download third-party libraries
wget "http://www.gisinternals.com/sdk/Download.aspx?file=release-1500-dev.zip" -O /tmp/release-1500-dev.zip
unzip /tmp/release-1500-dev.zip release-1500/* -d ~/.wine/drive_c

# Download and install Python
# wget http://www.python.org/ftp/python/2.7.3/python-2.7.3.msi -O /tmp/python-2.7.3.msi
# wine msiexec /i  /tmp/python-2.7.3.msi

# Checkout GDAL
svn checkout https://svn.osgeo.org/gdal/trunk $HOME/gdal_vce2008
ln -s $HOME/gdal_vce2008 ~/.wine/drive_c/gdal

# Install usefull scripts
cp build_vce2008.patch /tmp
cp build_vce2008.bat $HOME/gdal_vce2008/gdal
cp build_vce2008.sh $HOME/gdal_vce2008/gdal
cp clean_vce2008.bat $HOME/gdal_vce2008/gdal
cp clean_vce2008.sh $HOME/gdal_vce2008/gdal
cp nmake_vce2008.local $HOME/gdal_vce2008/gdal/nmake.local

cd ~/.wine/drive_c/gdal/gdal

# Makefile patches to workaround Wine issues
patch -p0 < /tmp/build_vce2008.patch

echo ""
echo "Installation finished !"
echo ""
echo "You can now run :"
echo "cd $HOME/gdal_vce2008/gdal"
echo "./build_vce2008.sh"
