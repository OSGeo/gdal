
include ../../../GDALmake.opt

OBJ	=	ogrgpsbabeldriver.o ogrgpsbabeldatasource.o ogrgpsbabelwritedatasource.o ogrgpsbabelfork.o

CPPFLAGS	:=	-I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_gpsbabel.h

