GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	EnvisatFile.o envisatdataset.o

default:	$(OBJ)

clean:
	rm -f *.o

%.o:	%.c
	$(CC) -c $(GDAL_INCLUDE) $(CFLAGS) $(XTRA_OPT) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CXXFLAGS) $(XTRA_OPT) $*.cpp -o $*.o


all:	$(OBJ)

install-obj:	$(foreach file,$(OBJ),../o/$(file))

../o/%.o:	%.o
	cp $*.o ../o

envisat_dump:	envisat_dump.o EnvisatFile.o
	$(CC) $(CFLAGS) envisat_dump.o EnvisatFile.o $(CPL_LIB) -ldl -lm -o envisat_dump

dumpgeo:	dumpgeo.o EnvisatFile.o
	$(CC) $(CFLAGS) dumpgeo.o EnvisatFile.o $(CPL_LIB) -ldl -lm -o dumpgeo

