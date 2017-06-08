
include ../GDALmake.opt

OBJ	=	gdalopeninfo.o gdaldrivermanager.o gdaldriver.o gdaldataset.o \
		gdalrasterband.o gdal_misc.o rasterio.o gdalrasterblock.o \
		gdalcolortable.o gdalmajorobject.o overview.o \
		gdaldefaultoverviews.o gdalpamdataset.o gdalpamrasterband.o \
		gdaljp2metadata.o gdaljp2box.o gdalmultidomainmetadata.o \
		gdal_rat.o gdalgmlcoverage.o gdalpamproxydb.o \
                gdalallvalidmaskband.o gdalnodatamaskband.o \
		gdalproxydataset.o gdalproxypool.o gdaldefaultasync.o \
		gdalnodatavaluesmaskband.o gdaldllmain.o gdalexif.o gdalclientserver.o \
		gdalgeorefpamdataset.o gdaljp2abstractdataset.o gdalvirtualmem.o \
		gdaloverviewdataset.o gdalrescaledalphaband.o gdaljp2structure.o \
		gdal_mdreader.o gdaljp2metadatagenerator.o gdalabstractbandblockcache.o \
		gdalarraybandblockcache.o gdalhashsetbandblockcache.o

CPPFLAGS	:=	 -I../frmts/gtiff -I../frmts/mem -I../frmts/vrt -I../ogr -I../ogr/ogrsf_frmts/generic -I../gnm/ -I../gnm/gnm_frmts/ $(JSON_INCLUDE) -I../ogr/ogrsf_frmts/geojson $(CPPFLAGS) $(PAM_SETTING) $(XTRA_OPT)

ifeq ($(HAVE_SQLITE),yes)
CXXFLAGS :=	$(CXXFLAGS) -DSQLITE_ENABLED
endif

ifeq ($(HAVE_LIBXML2),yes)
CXXFLAGS	:=	$(CXXFLAGS) $(LIBXML2_INC) -DHAVE_LIBXML2
endif

default: mdreader-target $(OBJ:.o=.$(OBJ_EXT)) rasterio_ssse3.$(OBJ_EXT)

rasterio_ssse3.$(OBJ_EXT):   rasterio_ssse3.cpp
	$(CXX) $(GDAL_INCLUDE) $(CXXFLAGS_NO_LTO_IF_SSSE3_NONDEFAULT) $(SSSE3FLAGS) $(CPPFLAGS) -c -o $@ $<

$(OBJ):	gdal_priv.h gdal_proxy.h

clean: mdreader-clean
	$(RM) *.o $(O_OBJ)

docs:
	(cd ..; $(MAKE) docs)

gdal_misc.$(OBJ_EXT):	gdal_misc.cpp gdal_version.h

gdaldrivermanager.$(OBJ_EXT):	gdaldrivermanager.cpp ../GDALmake.opt
	$(CXX) -c $(GDAL_INCLUDE) $(CPPFLAGS) $(CXXFLAGS) -DINST_DATA=\"$(INST_DATA)\" \
		$< -o $@

install:
	for f in *.h ; do $(INSTALL_DATA) $$f $(DESTDIR)$(INST_INCLUDE) ; done
