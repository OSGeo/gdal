GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	geotiff.o gt_wkt_srs.o gt_overview.o \
		tif_ovrcache.o tif_overview.o 

TIFF_OPTS	=	-Ilibgeotiff -Ilibtiff
SUBLIBS 	=

ifeq ($(TIFF_SETTING),internal)
SUBLIBS	:= lib-tiff $(SUBLIBS) 
endif

ifeq ($(GEOTIFF_SETTING),internal)
SUBLIBS	:= lib-geotiff $(SUBLIBS) 
endif

default:	$(OBJ) $(SUBLIBS)

clean:
	rm -f *.o
	(cd libtiff; $(MAKE) clean)
	(cd libgeotiff; $(MAKE) clean)

%.o:	%.c
	$(CC) -c $(CFLAGS) $(GDAL_INCLUDE) $(TIFF_OPTS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(CXXFLAGS) $(GDAL_INCLUDE) $(TIFF_OPTS) $*.cpp -o $*.o


gt_test:	gt_test.o gt_gs.o cpl_csv.o
	$(CC) gt_test.o gt_gs.o cpl_csv.o $(GDAL_ROOT)/port/*.o \
		libgeotiff/libgeotiff.a libtiff/libtiff.a $(LIBS) -o gt_test

gt_write:	gt_write.o gt_gs.o cpl_csv.o
	$(CC) gt_write.o gt_gs.o cpl_csv.o $(GDAL_ROOT)/port/*.o \
		libgeotiff/libgeotiff.a libtiff/libtiff.a $(LIBS) -o gt_write

epsg_to_wkt:	epsg_to_wkt.o gt_wkt_srs.o 
	$(CXX) epsg_to_wkt.o gt_wkt_srs.o $(GDAL_ROOT)/port/*.o \
	    libgeotiff/libgeotiff.a libtiff/libtiff.a \
	    $(OGR_LIB) $(CPL_LIB) $(LIBS) -o epsg_to_wkt

lib-tiff:
	(cd libtiff; $(MAKE) install-obj)

lib-geotiff:
	(cd libgeotiff; $(MAKE) install-obj)

install-obj:	$(SUBLIBS) \
		$(foreach file,$(OBJ),../o/$(file))

../o/%.o:	%.o
	cp $*.o ../o
