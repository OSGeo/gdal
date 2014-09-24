

include ../../../GDALmake.opt

OBJ	=	ogrxlsdriver.o ogrxlsdatasource.o ogrxlslayer.o

CPPFLAGS	:=	-I.. -I../.. $(FREEXL_INCLUDE) $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_xls.h