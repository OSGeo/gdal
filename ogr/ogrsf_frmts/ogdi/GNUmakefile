
GDAL_ROOT	=	../../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	ogrogdidriver.o ogrogdidatasource.o ogrogdilayer.o

CPPLAGS	+=	-I.. -I../.. $(OGDI_INCLUDE)

default:	$(O_OBJ)

$(OBJ):		ogrogdi.h

clean:
	rm -f *.o $(O_OBJ)
