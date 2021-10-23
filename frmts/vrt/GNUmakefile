include ../../GDALmake.opt

OBJ := vrtdataset.o vrtrasterband.o vrtdriver.o vrtsources.o
OBJ += vrtfilters.o vrtsourcedrasterband.o vrtrawrasterband.o
OBJ += vrtwarped.o vrtderivedrasterband.o vrtpansharpened.o
OBJ += pixelfunctions.o vrtmultidim.o

CPPFLAGS := $(CPPFLAGS)
CXXFLAGS := $(WARN_EFFCPLUSPLUS) $(WARN_OLD_STYLE_CAST) $(CXXFLAGS)

ifeq ($(HAVE_GEOS),yes)
CPPFLAGS        :=      -DHAVE_GEOS=1 $(CPPFLAGS)
endif


default: $(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj: $(O_OBJ:.o=.$(OBJ_EXT))

$(OBJ) $(O_OBJ): vrtdataset.h ../../alg/gdalwarper.h
$(OBJ) $(O_OBJ): ../../gcore/gdal_proxy.h

install:
	$(INSTALL_DATA) vrtdataset.h $(DESTDIR)$(INST_INCLUDE)
	$(INSTALL_DATA) gdal_vrt.h $(DESTDIR)$(INST_INCLUDE)
