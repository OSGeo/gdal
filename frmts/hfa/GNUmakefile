
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

HFAOBJ  =	hfaopen.o hfaentry.o hfadictionary.o hfafield.o hfatype.o \
		hfaband.o
OBJ	=	$(HFAOBJ) 

ALL_C_FLAGS	=	$(GDAL_INCLUDE) $(CFLAGS)

IMG_DATA_DIR	=	/usr2/data/imagine



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

img2tif:	img2tif.cpp imggeotiff.c geoextra.c $(HFAOBJ)
	$(CC) $(ALL_C_FLAGS) -I../geotiff/libtiff -I../geotiff/libgeotiff \
		img2tif.cpp imggeotiff.c geoextra.c \
		$(HFAOBJ) $(GDAL_ROOT)/port/*.o $(LIBS) \
		../geotiff/libgeotiff/libgeotiff.a \
		../geotiff/libtiff/libtiff.a \
		-o img2tif


#	
#	This test just dumps the Imagine data dictionary and node trees,
#	and compares to an existing set.
test:	hfatest
	./tst_hfa.sh $(IMG_DATA_DIR)/*.img $(IMG_DATA_DIR)/*/*.img

test_img2tif:	img2tif
	./tst_img2tif_1.sh
