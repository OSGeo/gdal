GDAL_ROOT	=	..

GDALMODULE	=	_gdalmodule.so

include ../GDALmake.opt

default:	$(GDALMODULE)

clean:
	rm -f *.o *.so

%.o:	%.c
	$(CC) -c $(CFLAGS) $(GDAL_INCLUDE) $(PYTHON_INCLUDES) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(CXXFLAGS) $(GDAL_INCLUDE) $(PYTHON_INCLUDES) $*.cpp -o $*.o


gdal_wrap.c:	gdal.i
	-swig -python gdal.i

$(GDAL_SLIB):

$(GDALMODULE):	gdal_wrap.o gdal_custom.o $(GDAL_LIB)
	$(LD_SHARED) gdal_wrap.o gdal_custom.o \
		$(GDAL_LIB) $(GDAL_LIBS) $(LIBS) -o $(GDALMODULE)


