
OBJ	=	EnvisatFile.o records.o envisatdataset.o

include ../../GDALmake.opt

XTRA_OPT =	-I../raw

CPPFLAGS	:=	$(GDAL_INCLUDE) $(XTRA_OPT) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o

all:	$(OBJ:.o=.$(OBJ_EXT))

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

$(O_OBJ):	../raw/rawdataset.h

envisat_dump:	envisat_dump.$(OBJ_EXT) EnvisatFile.$(OBJ_EXT)
	$(CC) $(CFLAGS) envisat_dump.$(OBJ_EXT) EnvisatFile.$(OBJ_EXT) $(GDAL_LIB) -ldl -lm -o envisat_dump

dumpgeo:	dumpgeo.$(OBJ_EXT) EnvisatFile.$(OBJ_EXT)
	$(CC) $(CFLAGS) dumpgeo.$(OBJ_EXT) EnvisatFile.$(OBJ_EXT) $(GDAL_LIB) -ldl -lm -o dumpgeo

