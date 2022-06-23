#!/bin/sh

set -eu

apt update -y
DEBIAN_FRONTEND=noninteractive apt install -y cmake libproj-dev wget python3-dev python3-numpy python3-pip

cd "$WORK_DIR"

if ! test -f l_dpcpp-cpp-compiler_p_2022.1.0.137_offline.sh; then
  wget https://registrationcenter-download.intel.com/akdlm/irc_nas/18717/l_dpcpp-cpp-compiler_p_2022.1.0.137_offline.sh
fi
sh l_dpcpp-cpp-compiler_p_2022.1.0.137_offline.sh -a -s --eula accept

pip3 install -r autotest/requirements.txt

mkdir build_icc
cd build_icc
. /opt/intel/oneapi/setvars.sh
CC=icc CXX=icx cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
ctest -j$(nproc) -V

