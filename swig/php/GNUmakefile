CC=gcc
CXX=g++
CFLAGS=-fpic
LDFLAGS=-shared
PHP_INC=`php-config --includes`
EXTRA_INC=-I../../../gcore -I../../../port -I../../../ogr -I../../../alg
EXTRA_LIB=-L../../../.libs -lgdal

GDAL_OBJS=gdal_php.o
GDAL_PROG=php_gdal.so

GDALCONST_OBJS=gdalconst_php.o
GDALCONST_PROG=php_gdalconst.so

OGR_OBJS=ogr_php.o
OGR_PROG=php_ogr.so

OSR_OBJS=osr_php.o
OSR_PROG=php_osr.so

default: $(GDAL_PROG) $(GDALCONST_PROG) $(OGR_PROG) $(OSR_PROG)

%.o: %.cpp
	$(CXX) $(EXTRA_INC) $(PHP_INC) $(CFLAGS) -c $<

%.o: %.c
	$(CC) $(EXTRA_INC) $(PHP_INC) $(CFLAGS) -c $<

php_%.so: %_php.o
	$(CC) $(LDFLAGS) $> -o $@ $(EXTRA_LIB)

