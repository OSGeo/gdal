
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

HFAOBJ  =	hfaopen.o hfaentry.o hfadictionary.o hfafield.o hfatype.o \
		hfaband.o
OBJ	=	$(HFAOBJ) hfadataset.o 

ALL_C_FLAGS	=	$(GDAL_INCLUDE) $(CFLAGS)

IMG_DATA_DIR	=	/usr2/data/imagine



default:	$(OBJ)

clean:
	rm -f *.o hfatest img2tif

%.o:	%.c
	$(CC) -c $(GDAL_INCLUDE) $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CXXFLAGS) $*.cpp -o $*.o

%.o:	addtiffo_src/%.cpp
	$(CXX) -c -I$(GDAL_ROOT)/gcore -I$(GDAL_ROOT)/port $(ALL_C_FLAGS) \
		addtiffo_src/$*.cpp -o $*.o


hfatest:	hfatest.o $(HFAOBJ)
	$(CXX) hfatest.o $(HFAOBJ) $(GDAL_ROOT)/port/*.o $(LIBS) -o hfatest

img2tif:	img2tif.cpp imggeotiff.c geoextra.c \
		tif_overview.o rawblockedimage.o $(HFAOBJ)
	$(CXX) $(ALL_C_FLAGS) -I../gtiff/libtiff -I../gtiff/libgeotiff \
		img2tif.cpp imggeotiff.c geoextra.c \
		$(HFAOBJ) $(GDAL_ROOT)/port/*.o $(LIBS) \
		tif_overview.o rawblockedimage.o\
		../gtiff/libgeotiff/*.o \
		../gtiff/libtiff/*.o \
		-o img2tif

#	
#	This test just dumps the Imagine data dictionary and node trees,
#	and compares to an existing set.
test:	hfatest
	./tst_hfa.sh $(IMG_DATA_DIR)/*.img \
		     $(IMG_DATA_DIR)/Eprj/*.img \
		     $(IMG_DATA_DIR)/fire/*.img \
		     $(IMG_DATA_DIR)/usf/*.img \
		     $(IMG_DATA_DIR)/colorado/*.img

test_img2tif:	img2tif
	./tst_img2tif_1.sh


install-obj:	$(foreach file,$(OBJ),../o/$(file))

../o/%.o:	%.o
	cp $*.o ../o

img2tif-dist:
	rm -rf img2tif_dist
	mkdir img2tif_dist
	cp img2tif_build.sh img2tif_dist/build.sh
	cp buildwin.bat img2tif_dist
	mkdir img2tif_dist/port
	cp $(GDAL_ROOT)/port/cpl_{config.h.vc,vsil_win32.cpp} img2tif_dist/port
	cp $(GDAL_ROOT)/port/cpl_{path.cpp,conv.cpp,conv.h,error.cpp,error.h,port.h,string.cpp,string.h,vsi.h,vsisimple.cpp,config.h,cpl_csv.cpp,cpl_findfile.cpp} img2tif_dist/port
	mkdir img2tif_dist/libtiff
	cp ../gtiff/libtiff/*.{c,h} img2tif_dist/libtiff
	mkdir img2tif_dist/libgeotiff
	cp ../gtiff/libgeotiff/*.{c,h,inc} img2tif_dist/libgeotiff
	mkdir img2tif_dist/hfa
	cp hfaopen.cpp hfaentry.cpp hfadictionary.cpp hfafield.cpp \
	   hfatype.cpp hfaband.cpp  \
	   hfa.h hfa_p.h img2tif_dist/hfa 
	cp BUILD.TXT hfatest.cpp img2tif.cpp imggeotiff.c \
	   addtiffo_src/tif_overview.cpp addtiffo_src/rawblocked* \
	   img2tif_dist
