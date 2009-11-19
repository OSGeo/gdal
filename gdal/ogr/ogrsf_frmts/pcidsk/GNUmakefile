

include ../../../GDALmake.opt

OBJ	=	ogrpcidskdriver.o ogrpcidskdatasource.o ogrpcidsklayer.o

CPPFLAGS	:=	-I.. -I../.. $(GDAL_INCLUDE) $(PCIDSK_INC) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_pcidsk.h
