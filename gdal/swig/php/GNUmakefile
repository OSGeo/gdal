

include ../../GDALmake.opt

all: 

BINDING = php
include ../SWIGmake.base

CC=gcc
CXX=g++
CFLAGS=-fpic
LDFLAGS=-shared
PHP_INC=`php-config --includes`
EXTRA_INC=-I../../port -I../../gcore -I../../alg -I../../ogr
EXTRA_LIB=

all_libs = php_gdal.so php_ogr.so php_gdalconst.so php_osr.so

PHP_MODULES   =       gdal.php gdalconst.php ogr.php osr.php
PHP_EXTRADIST =       php_gdal.h php_gdalconst.h php_ogr.h php_osr.h

clean:
	-rm -rf ${all_libs}

veryclean: clean
	-rm -f ${WRAPPERS} ${PHP_MODULES} ${PHP_EXTRADIST}

generate: ${WRAPPERS}

build: php_gdal.so

# This directive should actually do an install.
install: build

php_%.so: %_wrap.o
	$(CC) $(LDFLAGS) $< -o $@ $(EXTRA_LIB)

%.o: %.cpp
	$(CXX) $(EXTRA_INC) $(PHP_INC) $(CFLAGS) -c $<
%.o: %.cxx
	$(CXX) $(EXTRA_INC) $(PHP_INC) $(CFLAGS) -c $<
%.o: %.c
	$(CC) $(EXTRA_INC) $(PHP_INC) $(CFLAGS) -c $<
