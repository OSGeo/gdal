# $Id$
#
# Makefile to build OGR GeoJSON driver
#
include ../../../GDALmake.opt

ifeq ($(LIBJSONC_SETTING),internal)
SUBDIRS-yes	:= json-c
XTRA_OPT =	 -Ijson-c
endif

OBJ = \
	ogrgeojsondriver.o \
	ogrgeojsondatasource.o \
	ogrgeojsonlayer.o \
	ogrgeojsonwritelayer.o \
	ogrgeojsonutils.o \
	ogrgeojsonreader.o \
	ogrgeojsonwriter.o \
	ogresrijsonreader.o \
	ogrtopojsonreader.o

CPPFLAGS	:= -I. -I.. -I../.. $(XTRA_OPT) $(GDAL_INCLUDE) $(CPPFLAGS)

default:        $(foreach d,$(SUBDIRS-yes),$(d)-target) $(O_OBJ:.o=.$(OBJ_EXT))

clean: $(foreach d,$(SUBDIRS-yes),$(d)-clean)
	rm -f *.o $(O_OBJ)
	rm -f *~

$(O_OBJ):       ogr_geojson.h
