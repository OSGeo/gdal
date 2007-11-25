# $Id$
#
# Makefile to build OGR GeoJSON driver
#
SUBDIRS-yes	:= jsonc

include ../../../GDALmake.opt

OBJ = \
	ogrgeojsondriver.o \
	ogrgeojsondatasource.o \
	ogrgeojsonlayer.o \
	ogrgeojsonutils.o \
	ogrgeojsonreader.o \
	ogrgeojsonwriter.o

CPPFLAGS	:= -Ijsonc -I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS)

default:        $(foreach d,$(SUBDIRS-yes),$(d)-target) $(O_OBJ)

clean: $(foreach d,$(SUBDIRS-yes),$(d)-clean)
	rm -f *.o $(O_OBJ)
	rm -f *~
