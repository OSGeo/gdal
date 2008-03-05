

include ../../../GDALmake.opt

OBJ	=	ogrxplanedriver.o ogrxplanedatasource.o ogrxplanelayer.o ogr_xplane_geo_utils.o

CPPFLAGS	:=	-I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ)

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_xplane.h

test_geo_utils:	test_geo_utils.o ogr_xplane_geo_utils.o
	$(CXX) test_geo_utils.o ogr_xplane_geo_utils.o -o test_geo_utils -lm