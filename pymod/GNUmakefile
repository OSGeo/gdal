GDAL_ROOT	=	..

include ../GDALmake.opt

GDALMODULE	=	_gdalmodule.so
OBJ        	=       gdal_wrap.o numpydataset.o gdalnumeric.o

CPPFLAGS	+=	$(GDAL_INCLUDE) $(PYTHON_INCLUDES)
CFLAGS	+=	$(PYTHON_CFLAGS)

default:	$(GDALMODULE)

clean:
	$(RM) *.o *.so

gdal_wrap.c:	gdal.i
	-swig -python gdal.i

$(GDALMODULE):	$(OBJ) $(EXE_DEP_LIBS)
	$(LD_SHARED) $(OBJ) \
		$(GDAL_SLIB_LINK) $(PYTHON_LIBS) $(LIBS) -o $(GDALMODULE)

$(GDALMODULE:.so=.la):	$(OBJ:.o=.lo) $(EXE_DEP_LIBS)
	$(LD_SHARED) $^ $(PYTHON_LIBS) $(LIBS) -o $@

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

