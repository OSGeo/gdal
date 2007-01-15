

include ../../../GDALmake.opt

OBJ	=	ogrrecdriver.o ogrrecdatasource.o ogrreclayer.o ll_recio.o

CPPFLAGS	:=	-I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ)

clean:
	rm -f *.o $(O_OBJ)
