
GDAL_ROOT	=	../../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	ogrodbcdatasource.o ogrodbclayer.o ogrodbcdriver.o \
		ogrodbctablelayer.o

CPPFLAGS	+=	-I.. -I../..

default:	$(O_OBJ)

clean:
	rm -f *.o $(O_OBJ)
