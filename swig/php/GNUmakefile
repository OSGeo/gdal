CC=gcc
CXX=g++

CFLAGS=-fpic
LDFLAGS=-shared
PHP_INC=`php-config --includes`
EXTRA_INC=
EXTRA_LIB=

all: php_gdal.so php_ogr.so php_gdalconst.so php_osr.so

default:all

php_%.so: %_wrap.o
	$(CC) $(LDFLAGS) $< -o $@ $(EXTRA_LIB)

%.o: %.cpp
	$(CXX) $(EXTRA_INC) $(PHP_INC) $(CFLAGS) -c $<
%.o: %.cxx
	$(CXX) $(EXTRA_INC) $(PHP_INC) $(CFLAGS) -c $<
%.o: %.c
	$(CC) $(EXTRA_INC) $(PHP_INC) $(CFLAGS) -c $<
