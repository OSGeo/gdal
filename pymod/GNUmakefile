GDAL_ROOT	=	..

include ../GDALmake.opt

GDALMODULE	=	_gdalmodule
OBJ        	=       gdal_wrap.o numpydataset.o gdalnumeric.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(PYTHON_INCLUDES) $(CPPFLAGS)
CFLAGS		:=	$(PYTHON_CFLAGS) $(CFLAGS)
CXXFLAGS	:=	$(PYTHON_CFLAGS) $(CXXFLAGS)

PY_COMMANDS	=	epsg_tr.py gdalchksum.py gdal2xyz.py gcps2wld.py \
			gdalimport.py gdal_merge.py pct2rgb.py rgb2pct.py 
PY_MODULES	=	gdal.py ogr.py osr.py gdalconst.py gdalnumeric.py

# this is needed during install due to the _gdalmodule.so library
# being relinked by libtool
LIBS	+=	-L$(GDAL_ROOT)/.libs

default:	$(GDALMODULE).$(SO_EXT)

clean:
	$(RM) *.o *.$(SO_EXT) *.la

gdal_wrap.c:	gdal.i
	-swig -python gdal.i

$(GDALMODULE).$(SO_EXT):	$(OBJ) $(EXE_DEP_LIBS)
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
	for f in $(PY_COMMANDS) ; do $(INSTALL) $$f $(INST_BIN) ; done
	for f in $(PY_MODULES) ; do $(INSTALL_DATA) $$f $(INST_PYMOD) ; done
	# ugh! spurius relinking leaves a file owned by root into .libs/.
	rm -f .libs/*.soT
