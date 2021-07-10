
include ../../GDALmake.opt

OBJ	=	geotiff.o gt_wkt_srs.o gt_citation.o  gt_overview.o \
		tif_float.o tifvsi.o gt_jpeg_copy.o cogdriver.o

SUBLIBS 	=

TIFF_OPTS	=
ifeq ($(TIFF_SETTING),internal)

SUBLIBS	:= lib-tiff $(SUBLIBS)
TIFF_OPTS	:=	-DINTERNAL_LIBTIFF -Ilibtiff $(TIFF_OPTS)
ifeq ($(RENAME_INTERNAL_LIBTIFF_SYMBOLS),yes)
TIFF_OPTS	:=	-DRENAME_INTERNAL_LIBTIFF_SYMBOLS $(TIFF_OPTS)
endif

ifeq ($(LIBDEFLATE_SETTING),yes)
TIFF_OPTS 	:=	$(TIFF_OPTS) -DLIBDEFLATE_SUPPORT
endif

endif

ifeq ($(GEOTIFF_SETTING),internal)
SUBLIBS	:= lib-geotiff $(SUBLIBS)
TIFF_OPTS	:=	-DINTERNAL_LIBGEOTIFF -Ilibgeotiff $(TIFF_OPTS)
ifeq ($(RENAME_INTERNAL_LIBGEOTIFF_SYMBOLS),yes)
TIFF_OPTS	:=	-DRENAME_INTERNAL_LIBGEOTIFF_SYMBOLS $(TIFF_OPTS)
endif
endif

ifneq ($(JPEG_SETTING),no)
TIFF_OPTS 	:=	$(TIFF_OPTS) -I../jpeg -DHAVE_LIBJPEG
endif

ifeq ($(JPEG_SETTING),internal)
TIFF_OPTS 	:=	$(TIFF_OPTS) -I../jpeg/libjpeg
endif

CPPFLAGS	:=	 -iquote .. $(GEOTIFF_INCLUDE) $(PROJ_INCLUDE) $(PROJ_FLAGS) $(TIFF_OPTS) $(CPPFLAGS)

CXXFLAGS        :=      $(WARN_EFFCPLUSPLUS)  $(CXXFLAGS)
# issue with $(WARN_OLD_STYLE_CAST) with x86_64-w64-mingw32-g++. See https://travis-ci.org/OSGeo/gdal/jobs/375654138

default:	$(OBJ:.o=.$(OBJ_EXT)) $(SUBLIBS)

clean:
	rm -f *.o $(O_OBJ)
	(cd libtiff; $(MAKE) clean)
	(cd libgeotiff; $(MAKE) clean)

gt_test:	gt_test.$(OBJ_EXT) gt_gs.$(OBJ_EXT)
	$(CC) gt_test.$(OBJ_EXT) gt_gs.$(OBJ_EXT) ../../port/*.$(OBJ_EXT) \
		libgeotiff/libgeotiff.a libtiff/libtiff.a $(LIBS) -o gt_test

gt_write:	gt_write.$(OBJ_EXT) gt_gs.$(OBJ_EXT)
	$(CC) gt_write.$(OBJ_EXT) gt_gs.$(OBJ_EXT) ../../port/*.$(OBJ_EXT) \
		libgeotiff/libgeotiff.a libtiff/libtiff.a $(LIBS) -o gt_write

epsg_to_wkt:	epsg_to_wkt.$(OBJ_EXT) gt_wkt_srs.$(OBJ_EXT)
	$(CXX) epsg_to_wkt.$(OBJ_EXT) gt_wkt_srs.$(OBJ_EXT) ../../port/*.$(OBJ_EXT) \
	    libgeotiff/libgeotiff.a libtiff/libtiff.a \
	    $(GDAL_LIB) $(LIBS) -o epsg_to_wkt

lib-tiff:
	(cd libtiff; $(MAKE) install-obj)

lib-geotiff:
	(cd libgeotiff; $(MAKE) install-obj)

install-obj:	$(SUBLIBS) $(O_OBJ:.o=.$(OBJ_EXT))

generate_quant_table_md5sum: generate_quant_table_md5sum.cpp
	$(CXX) -O2 -g -Wall generate_quant_table_md5sum.cpp -o generate_quant_table_md5sum -iquote ../../port -iquote ../../gcore -iquote ../../ogr -L../../ -lgdal
	./generate_quant_table_md5sum > quant_table_md5sum.h
	$(RM) -f generate_quant_table_md5sum
