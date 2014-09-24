

include ../../../GDALmake.opt

OBJ	=	ogrgftdriver.o ogrgftdatasource.o ogrgftlayer.o ogrgfttablelayer.o ogrgftresultlayer.o

CPPFLAGS	:=	-I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_gft.h