
GDAL_ROOT	=	../../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	ogrodbcdatasource.o ogrodbclayer.o ogrodbcdriver.o \
		ogrodbctablelayer.o ogrodbcselectlayer.o

CPPFLAGS	:=	$(GDAL_INCLUDE) -I.. -I../.. $(CPPFLAGS)

default:	$(O_OBJ)

clean:
	rm -f *.o $(O_OBJ)
