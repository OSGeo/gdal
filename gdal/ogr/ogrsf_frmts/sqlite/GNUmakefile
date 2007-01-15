

include ../../../GDALmake.opt

OBJ	=	ogrsqlitedatasource.o ogrsqlitelayer.o ogrsqlitedriver.o \
		ogrsqlitetablelayer.o ogrsqliteselectlayer.o

CPPFLAGS	:=	-I.. $(GDAL_INCLUDE) $(SQLITE_INC) $(CPPFLAGS)

default:	$(O_OBJ)

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_sqlite.h
