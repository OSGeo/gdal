GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	geotiff.o geotiff_proj4.o

ALL_C_FLAGS	=	-Ilibgeotiff -Ilibtiff $(GDAL_INCLUDE) $(CFLAGS)



default:	$(OBJ)

clean:
	rm -f *.o
	(cd libtiff; $(MAKE) clean)
	(cd libgeotiff; $(MAKE) clean)

%.o:	%.cpp
	$(CC) -c $(ALL_C_FLAGS) -I$(GDAL_ROOT)/core -I$(GDAL_ROOT)/port \
		$*.cpp -o $*.o

%.o:	%.c
	$(CC) -c $(ALL_C_FLAGS) -I$(GDAL_ROOT)/core -I$(GDAL_ROOT)/port \
		$*.c -o $*.o


gt_test:	gt_test.o gt_gs.o cpl_csv.o
	$(CC) gt_test.o gt_gs.o cpl_csv.o $(GDAL_ROOT)/port/*.o \
		libgeotiff/libgeotiff.a libtiff/libtiff.a $(LIBS) -o gt_test

n_test:	n_test.o geotiff_proj4.o
	$(CC) n_test.o geotiff_proj4.o \
		$(GDAL_ROOT)/port/*.o \
		libgeotiff/libgeotiff.a libtiff/libtiff.a $(LIBS) -o n_test

gt_write:	gt_write.o gt_gs.o cpl_csv.o
	$(CC) gt_write.o gt_gs.o cpl_csv.o $(GDAL_ROOT)/port/*.o \
		libgeotiff/libgeotiff.a libtiff/libtiff.a $(LIBS) -o gt_write

addtifo:	addtifo.o tif_overview.o rawblockedimage.o
	$(CC) addtifo.o tif_overview.o rawblockedimage.o \
		$(GDAL_ROOT)/port/*.o \
		libgeotiff/libgeotiff.a libtiff/libtiff.a $(LIBS) -o addtifo


lib-tiff:
	cd libtiff; $(MAKE)

lib-geotiff:
	cd libgeotiff; $(MAKE)

install-obj:	$(OBJ) lib-tiff lib-geotiff
	cp $(OBJ) libgeotiff/libgeotiff.a libtiff/libtiff.a ../o
