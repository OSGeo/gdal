#!/bin/sh
set -x
#libtoolize --force --copy
aclocal -I ./m4
# We deliberately do not use autoheader, since it introduces PACKAGE junk
# that conflicts with other packages in cpl_config.h.in (FrankW)
#autoheader
autoconf
