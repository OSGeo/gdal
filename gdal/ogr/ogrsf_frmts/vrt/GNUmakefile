

include ../../../GDALmake.opt

OBJ	=	ogrvrtdatasource.o ogrvrtlayer.o ogrvrtdriver.o

CPPFLAGS	:=	-I.. -I../.. -I../generic $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):  ogr_vrt.h ../generic/ogrwarpedlayer.h ../generic/ogrunionlayer.h