
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

GDAL_OBJ	=	nitfdataset.o
NITFLIB_OBJ	=	nitffile.o nitfimage.o

OBJ = $(GDAL_OBJ) $(NITFLIB_OBJ)

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o

nitfdump:	nitfdump.o $(NITFLIB_OBJ)
	$(CXX) nitfdump.o $(NITFLIB_OBJ) $(GDAL_LIB) -o nitfdump

install-obj:	$(O_OBJ)
