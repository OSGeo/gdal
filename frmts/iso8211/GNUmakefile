
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt


INCLUDE	=	-I. -I$(GDAL_ROOT)/port

OBJ =	ddfmodule.o ddfutils.o ddffielddefn.o ddfrecord.o ddffield.o \
	ddfsubfielddefn.o

default:	$(OBJ) 8211dump

all:	8211dump docs

clean:
	rm -f *.o 8211dump

%.o:	%.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDE) $*.cpp -o $*.o

%.o:	%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $*.c -o $*.o


8211dump:	8211dump.o $(OBJ)
	$(CXX) 8211dump.o $(GDAL_ROOT)/port/*.o $(OBJ) $(LIBS) -o 8211dump

docs:
	doxygen
	rm html/index.html
	cp html/iso8211lib.html html/index.html



