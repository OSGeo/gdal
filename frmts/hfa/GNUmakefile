
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

HFAOBJ  =	hfaopen.o hfaentry.o hfadictionary.o hfafield.o hfatype.o \
		hfaband.o
OBJ	=	$(HFAOBJ) 

ALL_C_FLAGS	=	$(GDAL_INCLUDE) $(CFLAGS)



default:	$(OBJ)

clean:
	rm -f *.o hfatest

%.o:	%.cpp
	$(CC) -c $(ALL_C_FLAGS) -I$(GDAL_ROOT)/core -I$(GDAL_ROOT)/port \
		$*.cpp -o $*.o

%.o:	%.c
	$(CC) -c $(ALL_C_FLAGS) -I$(GDAL_ROOT)/core -I$(GDAL_ROOT)/port \
		$*.c -o $*.o


hfatest:	hfatest.o $(HFAOBJ)
	$(CC) hfatest.o $(HFAOBJ) $(GDAL_ROOT)/port/*.o $(LIBS) -o hfatest

img2tif:	img2tif.c imggeotiff.c rrd2tif.cpp geoextra.c $(HFAOBJ)
	$(CC) $(ALL_C_FLAGS) -I../geotiff/libtiff -I../geotiff/libgeotiff \
		img2tif.c imggeotiff.c rrd2tif.cpp geoextra.c \
		$(HFAOBJ) $(GDAL_ROOT)/port/*.o $(LIBS) \
		../geotiff/libgeotiff/libgeotiff.a \
		../geotiff/libtiff/libtiff.a \
		-o img2tif
