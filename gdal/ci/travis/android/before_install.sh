#!/bin/sh

set -e

wget http://dl.google.com/android/ndk/android-ndk-r9d-linux-x86_64.tar.bz2
tar xjf android-ndk-r9d-linux-x86_64.tar.bz2
android-ndk-r9d/build/tools/make-standalone-toolchain.sh --system=linux-x86_64 --platform=android-8 --install-dir=$HOME/android-8-toolchain
