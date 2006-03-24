

include ../../../GDALmake.opt

OBJ	=	ogrmemdriver.o ogrmemdatasource.o ogrmemlayer.o

CPPFLAGS	:=	-I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ)

clean:
	rm -f *.o $(O_OBJ)
