

include ../../../GDALmake.opt

OBJ	=	ogrvrtdatasource.o ogrvrtlayer.o ogrvrtdriver.o

CPPFLAGS	:=	-I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ)

clean:
	rm -f *.o $(O_OBJ)
