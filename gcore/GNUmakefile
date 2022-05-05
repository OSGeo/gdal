
include ../GDALmake.opt

OBJ	=	gdalmultidim.o gdalopeninfo.o gdaldrivermanager.o gdaldriver.o gdaldataset.o \
		gdalrasterband.o gdal_misc.o rasterio.o gdalrasterblock.o \
		gdalcolortable.o gdalmajorobject.o overview.o \
		gdaldefaultoverviews.o gdalpamdataset.o gdalpamrasterband.o \
		gdaljp2metadata.o gdaljp2box.o gdalmultidomainmetadata.o \
		gdal_rat.o gdalpamproxydb.o \
                gdalallvalidmaskband.o gdalnodatamaskband.o \
		gdalproxydataset.o gdalproxypool.o gdaldefaultasync.o \
		gdalnodatavaluesmaskband.o gdaldllmain.o gdalexif.o \
		gdalgeorefpamdataset.o gdaljp2abstractdataset.o gdalvirtualmem.o \
		gdaloverviewdataset.o gdalrescaledalphaband.o gdaljp2structure.o \
		gdal_mdreader.o gdaljp2metadatagenerator.o gdalabstractbandblockcache.o \
		gdalarraybandblockcache.o gdalhashsetbandblockcache.o rawdataset.o \
		gdalpython.o gdalpythondriverloader.o tilematrixset.o \
		gdal_thread_pool.o

CPPFLAGS	:=	 -iquote ../frmts/gtiff -iquote ../frmts/mem -iquote ../frmts/vrt -iquote ../ogr -iquote ../ogr/ogrsf_frmts/generic -iquote ../gnm/ -iquote ../gnm/gnm_frmts/ $(JSON_INCLUDE) -iquote ../ogr/ogrsf_frmts/geojson $(CPPFLAGS) $(PROJ_INCLUDE) $(PAM_SETTING) $(XTRA_OPT)

ifeq ($(HAVE_SQLITE),yes)
CPPFLAGS :=	$(CPPFLAGS) -DSQLITE_ENABLED
endif

ifeq ($(HAVE_LIBXML2),yes)
CPPFLAGS	:=	$(CPPFLAGS) $(LIBXML2_INC) -DHAVE_LIBXML2
endif

ifeq ($(HAVE_GEOS),yes)
CPPFLAGS 	:=	$(CPPFLAGS) -DHAVE_GEOS=1 $(GEOS_CFLAGS)
endif

#nasakeywordhandler is used by pds and hdf4
compile_nasakeywordhandler = no
ifeq ($(HAVE_HDF4),yes)
  compile_nasakeywordhandler = yes
endif
ifeq ($(shell echo $(GDAL_FORMATS) | grep "pds" >/dev/null && echo "yes" || echo "no"),yes)
  compile_nasakeywordhandler = yes
endif
ifeq ($(compile_nasakeywordhandler),yes)
    OBJ += nasakeywordhandler.o
endif

CXXFLAGS        :=      $(WARN_EFFCPLUSPLUS) $(WARN_OLD_STYLE_CAST) $(CXXFLAGS)

GENERATE_GDAL_VERSION_H := $(shell ./generate_gdal_version_h.sh)

default: mdreader-target $(OBJ:.o=.$(OBJ_EXT)) rasterio_ssse3.$(OBJ_EXT)

.PHONY: generate_gdal_version_h

generate_gdal_version_h: ;

rasterio_ssse3.$(OBJ_EXT):   rasterio_ssse3.cpp
	$(CXX) $(GDAL_INCLUDE) $(CXXFLAGS_NO_LTO_IF_SSSE3_NONDEFAULT) $(SSSE3FLAGS) $(CPPFLAGS) -c -o $@ $<

$(OBJ):	gdal_priv.h gdal_proxy.h

clean: mdreader-clean
	$(RM) *.o $(O_OBJ)

docs:
	(cd ..; $(MAKE) docs)

gdal_misc.$(OBJ_EXT):	gdal_misc.cpp ../generated_headers/gdal_version.h

gdaldrivermanager.$(OBJ_EXT):	gdaldrivermanager.cpp ../GDALmake.opt
	$(CXX) -c $(GDAL_INCLUDE) $(CPPFLAGS) $(CXXFLAGS) -DINST_DATA=\"$(INST_DATA)\" \
		$< -o $@

INST_H_FILES = \
	gdal_frmts.h \
	gdalgeorefpamdataset.h \
	gdal.h \
	gdaljp2abstractdataset.h \
	gdaljp2metadata.h \
	gdal_mdreader.h \
	gdal_pam.h \
	gdal_priv.h \
	gdal_proxy.h \
	gdal_rat.h \
	gdalcachedpixelaccessor.h \
	rawdataset.h

install:
	for f in $(INST_H_FILES) ; do $(INSTALL_DATA) $$f $(DESTDIR)$(INST_INCLUDE) ; done
