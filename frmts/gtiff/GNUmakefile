GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	cpl_csv.o gt_gs.o geotiff.o

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

gt_write:	gt_write.o gt_gs.o cpl_csv.o
	$(CC) gt_write.o gt_gs.o cpl_csv.o $(GDAL_ROOT)/port/*.o \
		libgeotiff/libgeotiff.a libtiff/libtiff.a $(LIBS) -o gt_write


lib-tiff:
	cd libtiff; $(MAKE)

lib-geotiff:
	cd libgeotiff; $(MAKE)

install-obj:	$(OBJ) lib-tiff lib-geotiff
	cp $(OBJ) libgeotiff/libgeotiff.a libtiff/libtiff.a ../o

