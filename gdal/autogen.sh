#!/bin/sh
set -x
#libtoolize --force --copy
aclocal -I ./m4
autoheader
autoconf
