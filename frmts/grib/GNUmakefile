

EXTRAFLAGS = -iquote degrib/degrib -iquote ../mem

include ../../GDALmake.opt

OBJ	=	gribdataset.o gribcreatecopy.o \
                clock.o \
                degrib1.o \
                degrib2.o inventory.o metaname.o myerror.o grib1tab.o myutil.o metaparse.o weather.o metaprint.o grib2api.o myassert.o scan.o tendian.o gridtemplates.o drstemplates.o pdstemplates.o gbits.o g2_free.o g2_unpack1.o g2_unpack2.o g2_unpack3.o g2_unpack4.o g2_unpack5.o g2_unpack6.o g2_unpack7.o g2_info.o g2_getfld.o simunpack.o comunpack.o pack_gp.o reduce.o specunpack.o rdieee.o mkieee.o int_power.o simpack.o compack.o cmplxpack.o misspack.o \
		dec_jpeg2000.o jpcunpack.o hazard.o

ifeq ($(PNG_SETTING),internal)
EXTRAFLAGS      :=      $(EXTRAFLAGS) -I../png/libpng -DUSE_PNG

ifeq ($(RENAME_INTERNAL_PNG_SYMBOLS),yes)
EXTRAFLAGS 	:=	$(EXTRAFLAGS) -DRENAME_INTERNAL_PNG_SYMBOLS
endif

ifeq ($(LIBZ_SETTING),internal)
EXTRAFLAGS      :=      $(EXTRAFLAGS) -I../zlib
endif
OBJ     :=      $(OBJ) pngunpack.o dec_png.o
endif

ifeq ($(PNG_SETTING),external)
EXTRAFLAGS      :=      $(EXTRAFLAGS) -DUSE_PNG
OBJ     :=      $(OBJ) pngunpack.o dec_png.o
endif

CPPFLAGS	:=	 $(EXTRAFLAGS) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

../o/%.$(OBJ_EXT):	degrib/degrib/%.c
	$(CC) -c $(GDAL_INCLUDE) $(CPPFLAGS) $(CFLAGS) -iquote degrib/g2clib $< -o $@

../o/%.$(OBJ_EXT):	degrib/degrib/%.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

../o/%.$(OBJ_EXT):	degrib/g2clib/%.c
	$(CC) -c $(GDAL_INCLUDE) $(CPPFLAGS) $(CFLAGS) $< -o $@

../o/%.$(OBJ_EXT):	degrib/g2clib/%.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

%.$(OBJ_EXT):	degrib/degrib/%.c
	$(CC) -c $(GDAL_INCLUDE) $(CPPFLAGS) $(CFLAGS) -iquote degrib/g2clib $< -o $@

%.$(OBJ_EXT):	degrib/degrib/%.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

%.$(OBJ_EXT):	degrib/g2clib/%.c
	$(CC) -c $(GDAL_INCLUDE) $(CPPFLAGS) $(CFLAGS) $< -o $@

%.$(OBJ_EXT):	degrib/g2clib/%.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

defaultold:	$(OBJ:.o=.$(OBJ_EXT))
	(cd degrib/degrib; $(MAKE)); cd ../..
	(cd degrib/g2clib; $(MAKE))
clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

plugin: gdal_GRIB.so

gdal_GRIB.so:	$(OBJ)
	$(LD_SHARED) $(OBJ) $(GDAL_LIBS) $(LIBS) -o gdal_GRIB.so



