
GDAL_ROOT	=	../../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	ogrmemdriver.o ogrmemdatasource.o ogrmemlayer.o

CPPFLAGS	+=	-I.. -I../..

default:	$(O_OBJ)

clean:
	rm -f *.o
