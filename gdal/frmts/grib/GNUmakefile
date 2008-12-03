

EXTRAFLAGS = -Idegrib18/degrib

include ../../GDALmake.opt

OBJ	=	gribdataset.o \
                clock.o \
                degrib1.o \
                degrib2.o inventory.o metaname.o myerror.o tdlpack.o filedatasource.o memorydatasource.o grib1tab.o myutil.o metaparse.o weather.o metaprint.o engribapi.o grib2api.o myassert.o scan.o memendian.o fileendian.o gridtemplates.o drstemplates.o pdstemplates.o gbits.o g2_free.o g2_unpack1.o g2_unpack2.o g2_unpack3.o g2_unpack4.o g2_unpack5.o g2_unpack6.o g2_unpack7.o g2_info.o g2_getfld.o simunpack.o comunpack.o pack_gp.o reduce.o specpack.o specunpack.o rdieee.o mkieee.o int_power.o simpack.o compack.o cmplxpack.o misspack.o g2_create.o g2_addlocal.o g2_addgrid.o g2_addfield.o g2_gribend.o getdim.o g2_miss.o getpoly.o seekgb.o \
		dec_jpeg2000.o jpcunpack.o jpcpack.o enc_jpeg2000.o

ifeq ($(HAVE_JASPER),yes)
EXTRAFLAGS	:= 	$(EXTRAFLAGS) -DHAVE_JASPER
endif

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) $(EXTRAFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

../o/%.$(OBJ_EXT):	degrib18/degrib/%.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) -Idegrib18/g2clib-1.0.4 $< -o $@

../o/%.$(OBJ_EXT):	degrib18/degrib/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

../o/%.$(OBJ_EXT):	degrib18/g2clib-1.0.4/%.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

../o/%.$(OBJ_EXT):	degrib18/g2clib-1.0.4/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

%.$(OBJ_EXT):	degrib18/degrib/%.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) -Idegrib18/g2clib-1.0.4 $< -o $@

%.$(OBJ_EXT):	degrib18/degrib/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

%.$(OBJ_EXT):	degrib18/g2clib-1.0.4/%.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

%.$(OBJ_EXT):	degrib18/g2clib-1.0.4/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

defaultold:	$(OBJ:.o=.$(OBJ_EXT))
	(cd degrib18/degrib; $(MAKE)); cd ../..
	(cd degrib18/g2clib-1.0.4; $(MAKE))
clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

plugin: gdal_GRIB.so

gdal_GRIB.so:	$(OBJ)
	$(LD_SHARED) $(OBJ) $(GDAL_LIBS) $(LIBS) -o gdal_GRIB.so



