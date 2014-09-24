

include ../../../GDALmake.opt

OBJ	=	ogrpgdumpdriver.o ogrpgdumpdatasource.o ogrpgdumplayer.o

CPPFLAGS	:=	-I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_pgdump.h