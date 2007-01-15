
OBJ	=	EnvisatFile.o envisatdataset.o

include ../../GDALmake.opt

XTRA_OPT =	-I../raw

CPPFLAGS	:=	$(GDAL_INCLUDE) $(XTRA_OPT) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o

all:	$(OBJ)

install-obj:	$(O_OBJ)

envisat_dump:	envisat_dump.o EnvisatFile.o
	$(CC) $(CFLAGS) envisat_dump.o EnvisatFile.o $(GDAL_LIB) -ldl -lm -o envisat_dump

dumpgeo:	dumpgeo.o EnvisatFile.o
	$(CC) $(CFLAGS) dumpgeo.o EnvisatFile.o $(GDAL_LIB) -ldl -lm -o dumpgeo

