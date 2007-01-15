

include ../../../GDALmake.opt

OBJ	=	ogrpgeodatasource.o ogrpgeolayer.o ogrpgeodriver.o \
		ogrpgeotablelayer.o ogrpgeoselectlayer.o

CPPFLAGS	:=	$(GDAL_INCLUDE) -I.. -I../.. $(CPPFLAGS)

default:	$(O_OBJ)

clean:
	rm -f *.o $(O_OBJ)
