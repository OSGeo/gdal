GDAL_ROOT	=	..

include ../GDALmake.opt

GDALMODULE	=	_gdalmodule
OBJ        	=       gdal_wrap.o numpydataset.o gdalnumeric.o

CPPFLAGS	+=	$(GDAL_INCLUDE) $(PYTHON_INCLUDES)
CFLAGS	+=	$(PYTHON_CFLAGS)

# this is needed during install due to the _gdalmodule.so library
# being relinked by libtool
LIBS	+=	-L$(GDAL_ROOT)/.libs

default:	$(GDALMODULE).$(SO_EXT)

clean:
	$(RM) *.o *.so *.la

gdal_wrap.c:	gdal.i
	-swig -python gdal.i

$(GDALMODULE).so:	$(OBJ) $(EXE_DEP_LIBS)
	$(LD_SHARED) $(OBJ) \
		$(GDAL_SLIB_LINK) $(PYTHON_LIBS) $(LIBS) -o $@

%.lo:	%.o ;

$(GDALMODULE).la:	$(OBJ:.o=.lo) $(EXE_DEP_LIBS)
	$(LD) $^ $(PYTHON_LIBS) $(LIBS) -o $@ \
		-rpath $(INST_PYMOD) \
		-no-undefined \
		-avoid-version \
		-module

# special care is taken so that python scripts are installed as executables
install:	$(GDALMODULE).$(SO_EXT)
	$(INSTALL_DIR) $(INST_PYMOD)
	$(INSTALL_LIB) $(GDALMODULE).$(SO_EXT) $(INST_PYMOD)
	for f in *.py ; do \
	    if egrep '\#\!.*python' $$f > /dev/null ; then \
		$(INSTALL) $$f $(INST_PYMOD) ; \
	    else \
		$(INSTALL_DATA) $$f $(INST_PYMOD) ; \
	    fi \
	done
	# ugh! spurius relinking leaves a file owned by root into .libs/.
	find . -user root -exec rm -f '{}' ';'
