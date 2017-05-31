

include ../../../GDALmake.opt

OBJ	=	ogrxplanedriver.o ogrxplanedatasource.o ogrxplanelayer.o \
		ogr_xplane_reader.o ogr_xplane_apt_reader.o ogr_xplane_nav_reader.o \
 		ogr_xplane_fix_reader.o ogr_xplane_awy_reader.o

CPPFLAGS	:=	-I.. -I../..  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_xplane.h ogr_xplane_reader.h \
		ogr_xplane_apt_reader.h ogr_xplane_nav_reader.h \
		ogr_xplane_fix_reader.h ogr_xplane_awy_reader.h ../../ogr_geometry.h
