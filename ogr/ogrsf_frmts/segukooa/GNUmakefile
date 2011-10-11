

include ../../../GDALmake.opt

OBJ	=	ogrsegukooadriver.o ogrsegukooadatasource.o ogrsegukooalayer.o

CPPFLAGS	:=	-I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_segukooa.h