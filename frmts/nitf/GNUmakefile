
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

GDAL_OBJ	=	nitfdataset.o
NITFLIB_OBJ	=	nitffile.o nitfimage.o

OBJ = $(GDAL_OBJ) $(NITFLIB_OBJ)

default:	$(OBJ)

clean:
	rm -f *.o

%.o:	%.c
	$(CC) -c $(GDAL_INCLUDE) $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CXXFLAGS) $*.cpp -o $*.o


nitfdump:	nitfdump.o $(NITFLIB_OBJ)
	$(CXX) nitfdump.o $(NITFLIB_OBJ) $(GDAL_LIB) -o nitfdump

install-obj:	$(foreach file,$(OBJ),../o/$(file))
