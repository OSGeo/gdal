GDAL_ROOT	=	..

GDALMODULE	=	_gdalmodule.so
OBJ        	=       gdal_wrap.o numpydataset.o gdalnumeric.o

include ../GDALmake.opt

default:	$(GDALMODULE)

clean:
	rm -f *.o *.so

%.o:	%.c
	$(CC) -c  $(GDAL_INCLUDE) $(PYTHON_INCLUDES)\
		$(CFLAGS) $(PYTHON_CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(PYTHON_INCLUDES) \
		 $(CXXFLAGS) $(PYTHON_CFLAGS) $*.cpp -o $*.o


gdal_wrap.c:	gdal.i
	-swig -python gdal.i

$(GDAL_SLIB):

$(GDALMODULE):	$(OBJ) $(GDAL_LIB)
	$(LD_SHARED) $(OBJ) \
		$(GDAL_SLIB_LINK) $(PYTHON_LIBS) $(LIBS) -o $(GDALMODULE)

# special care is taken so that python scripts are installed as executables
install:	$(GDALMODULE)
	$(INSTALL_DIR) $(INST_PYMOD)
	$(INSTALL_DATA) $(GDALMODULE) $(INST_PYMOD)
	for f in *.py ; do \
	    if egrep '\#\!.*python' $$f > /dev/null ; then \
		$(INSTALL) $$f $(INST_PYMOD) ; \
	    else \
		$(INSTALL_DATA) $$f $(INST_PYMOD) ; \
	    fi \
	done

