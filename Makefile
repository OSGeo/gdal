##############################################################################
#
# Project:  GDAL
# Purpose:  Implementation of a set of GDALDerivedPixelFunc(s) to be used
#           with source raster band of virtual GDAL datasets.
# Author:   Antonio Valentino <antonio.valentino@tiscali.it>
#
##############################################################################
# Copyright (c) 2008-2014 Antonio Valentino <antonio.valentino@tiscali.it>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
##############################################################################

.PHONY: all clean dist check

OBJS = pixfunplugin.o pixelfunctions.o
CFLAGS := -fPIC -Wall -Wno-long-long $(shell gdal-config --cflags) $(CFLAGS)
CFLAGS := -pedantic $(CFLAGS)

CFLAGS := -g $(CFLAGS)
#CFLAGS := -O3  $(CFLAGS)

TARGET = gdal_PIXFUN.so

all: $(TARGET)

clean:
	$(RM) $(TARGET) *.o *~
	$(RM) autotest/pymod/*.pyc autotest/gcore/*.pyc

$(TARGET): $(OBJS)
	$(CC) -shared -o $@ $(OBJS) $(shell gdal-config --libs)


PYTHON=python

check: $(TARGET)
	cd autotest/gcore && \
	env GDAL_DRIVER_PATH=$(PWD):$(GDAL_DRIVER_PATH) \
	PYTHONPATH=$(PWD)/autotest/pymod:$(PYTHONPATH) \
	$(PYTHON) pixfun.py
