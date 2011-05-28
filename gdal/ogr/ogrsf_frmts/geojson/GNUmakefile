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
	ogrgeojsonwriter.o \
	ogresrijsonreader.o

CPPFLAGS	:= -I. -Ijsonc -I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS)

default:        $(foreach d,$(SUBDIRS-yes),$(d)-target) $(O_OBJ:.o=.$(OBJ_EXT))

clean: $(foreach d,$(SUBDIRS-yes),$(d)-clean)
	rm -f *.o $(O_OBJ)
	rm -f *~

$(O_OBJ):       ogr_geojson.h
