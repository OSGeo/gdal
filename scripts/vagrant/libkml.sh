NUMTHREADS=2
if [[ -f /sys/devices/system/cpu/online ]]; then
	# Calculates 1.5 times physical threads
	NUMTHREADS=$(( ( $(cut -f 2 -d '-' /sys/devices/system/cpu/online) + 1 ) * 15 / 10  ))
fi
#NUMTHREADS=1 # disable MP
export NUMTHREADS

#svn checkout http://libkml.googlecode.com/svn/trunk/ libkml-read-only
#cd libkml-read-only
#git clone https://github.com/google/libkml.git
svn co https://github.com/google/libkml/trunk libkml
cd libkml
./autogen.sh
./configure

make -j $NUMTHREADS
sudo make install

cd ..
