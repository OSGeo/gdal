GDAL_ROOT	=	..

include ../GDALmake.opt

OBJ	=	gdalmediancut.o gdaldither.o

default:	$(OBJ)

clean:
	rm -f *.o

docs:	
	(cd ..; $(MAKE) docs)

%.o:	%.c
	$(CC) -c $(GDAL_INCLUDE) $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CXXFLAGS) $*.cpp -o $*.o

install:
	for f in *.h ; do $(INSTALL) $$f $(INST_INCLUDE) ; done
