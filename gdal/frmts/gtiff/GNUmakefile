
include ../../GDALmake.opt

OBJ	=	geotiff.o gt_wkt_srs.o gt_overview.o \
		tif_ovrcache.o tif_overview.o tif_float.o tifvsi.o

TIFF_OPTS	=	
SUBLIBS 	=

ifeq ($(TIFF_SETTING),internal)
SUBLIBS	:= lib-tiff $(SUBLIBS) 
TIFF_OPTS	:=	-Ilibtiff $(TIFF_OPTS)
endif

ifeq ($(GEOTIFF_SETTING),internal)
SUBLIBS	:= lib-geotiff $(SUBLIBS) 
TIFF_OPTS	:=	-Ilibgeotiff $(TIFF_OPTS)
endif

CPPFLAGS	:=	$(GDAL_INCLUDE) $(TIFF_OPTS) $(CPPFLAGS)

default:	$(OBJ) $(SUBLIBS)

clean:
	rm -f *.o $(O_OBJ)
	(cd libtiff; $(MAKE) clean)
	(cd libgeotiff; $(MAKE) clean)

gt_test:	gt_test.o gt_gs.o cpl_csv.o
	$(CC) gt_test.o gt_gs.o cpl_csv.o ../../port/*.o \
		libgeotiff/libgeotiff.a libtiff/libtiff.a $(LIBS) -o gt_test

gt_write:	gt_write.o gt_gs.o cpl_csv.o
	$(CC) gt_write.o gt_gs.o cpl_csv.o ../../port/*.o \
		libgeotiff/libgeotiff.a libtiff/libtiff.a $(LIBS) -o gt_write

epsg_to_wkt:	epsg_to_wkt.o gt_wkt_srs.o 
	$(CXX) epsg_to_wkt.o gt_wkt_srs.o ../../port/*.o \
	    libgeotiff/libgeotiff.a libtiff/libtiff.a \
	    $(GDAL_LIB) $(LIBS) -o epsg_to_wkt

lib-tiff:
	(cd libtiff; $(MAKE) install-obj)

lib-geotiff:
	(cd libgeotiff; $(MAKE) install-obj)

install-obj:	$(SUBLIBS) $(O_OBJ)
