
GDAL_ROOT	=	../../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	ogrvrtdatasource.o ogrvrtlayer.o ogrvrtdriver.o

CPPFLAGS	+=	-I.. -I../..

default:	$(O_OBJ)

clean:
	rm -f *.o $(O_OBJ)
