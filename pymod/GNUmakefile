GDAL_ROOT	=	..

GDALMODULE	=	_gdalmodule.so
OBJ        	=       gdal_wrap.o numpydataset.o gdalnumeric.o

include ../GDALmake.opt

default:	$(GDALMODULE)

clean:
	rm -f *.o *.so

%.o:	%.c
	$(CC) -c $(CFLAGS) $(NUMPY_FLAG) \
		$(GDAL_INCLUDE) $(PYTHON_INCLUDES) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(CXXFLAGS) $(NUMPY_FLAG) \
		$(GDAL_INCLUDE) $(PYTHON_INCLUDES) $*.cpp -o $*.o


gdal_wrap.c:	gdal.i
	-swig -python gdal.i

$(GDAL_SLIB):

$(GDALMODULE):	$(OBJ) $(GDAL_LIB)
	$(LD_SHARED) $(OBJ) \
		$(GDAL_SLIB_LINK) $(LIBS) -o $(GDALMODULE)


install:
	$(INSTALL) -d $(INST_PYMOD)
	$(INSTALL) *.py $(GDALMODULE) $(INST_PYMOD)
