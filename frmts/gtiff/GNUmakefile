GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	geotiff.o gt_wkt_srs.o

ALL_C_FLAGS	=	-Ilibgeotiff -Ilibtiff $(GDAL_INCLUDE) $(CFLAGS)



default:	$(OBJ)

clean:
	rm -f *.o
	(cd libtiff; $(MAKE) clean)
	(cd libgeotiff; $(MAKE) clean)

%.o:	%.cpp
	$(CXX) -c $(ALL_C_FLAGS) -I$(GDAL_ROOT)/core -I$(GDAL_ROOT)/port \
		$*.cpp -o $*.o

%.o:	%.c
	$(CC) -c $(ALL_C_FLAGS) -I$(GDAL_ROOT)/core -I$(GDAL_ROOT)/port \
		$*.c -o $*.o


gt_test:	gt_test.o gt_gs.o cpl_csv.o
	$(CC) gt_test.o gt_gs.o cpl_csv.o $(GDAL_ROOT)/port/*.o \
		libgeotiff/libgeotiff.a libtiff/libtiff.a $(LIBS) -o gt_test

n_test:	n_test.o
	$(CC) n_test.o \
		$(GDAL_ROOT)/port/*.o \
		libgeotiff/libgeotiff.a libtiff/libtiff.a $(LIBS) -o n_test

gt_write:	gt_write.o gt_gs.o cpl_csv.o
	$(CC) gt_write.o gt_gs.o cpl_csv.o $(GDAL_ROOT)/port/*.o \
		libgeotiff/libgeotiff.a libtiff/libtiff.a $(LIBS) -o gt_write

epsg_to_wkt:	epsg_to_wkt.o gt_wkt_srs.o 
	$(CXX) epsg_to_wkt.o gt_wkt_srs.o $(GDAL_ROOT)/port/*.o \
	    libgeotiff/libgeotiff.a libtiff/libtiff.a \
	    $(OGR_LIB) $(CPL_LIB) $(LIBS) -o epsg_to_wkt

addtiffo:	addtiffo.o tif_overview.o rawblockedimage.o
	$(CC) addtiffo.o tif_overview.o rawblockedimage.o \
		$(GDAL_ROOT)/port/*.o \
		libgeotiff/libgeotiff.a libtiff/libtiff.a $(LIBS) -o addtiffo


lib-tiff:
	(cd libtiff; $(MAKE))

lib-geotiff:
	(cd libgeotiff; $(MAKE))

all:	lib-tiff lib-geotiff

install-obj:	../o/libtiff.a ../o/libgeotiff.a \
		$(foreach file,$(OBJ),../o/$(file))

../o/%.o:	%.o
	cp $*.o ../o

../o/libtiff.a:	libtiff/libtiff.a
	cp libtiff/libtiff.a ../o

libtiff/libtiff.a:	lib-tiff

../o/libgeotiff.a:	libgeotiff/libgeotiff.a
	cp libgeotiff/libgeotiff.a ../o

libgeotiff/libgeotiff.a:	lib-geotiff


addtiffo-dist:
	rm -rf addtiffo
	mkdir addtiffo
	cp tif_overview.cpp addtiffo.c rawblockedimage.* addtiffo
	cp README.addtiffo addtiffo/README
	cp Makefile.addtiffo addtiffo/Makefile
	cp makefile.vc.addtiffo addtiffo/Makefile.vc

