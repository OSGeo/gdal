

include ../../GDALmake.opt

HFAOBJ  =	hfaopen.o hfaentry.o hfadictionary.o hfafield.o hfatype.o \
		hfaband.o hfacompress.o
OBJ	=	$(HFAOBJ) hfadataset.o hfa_overviews.o

ALL_C_FLAGS	=	$(GDAL_INCLUDE) $(CFLAGS)

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS)

IMG_DATA_DIR	=	/usr2/data/imagine

default:	$(OBJ)

clean:
	rm -f *.o hfatest img2tif $(O_OBJ)

$(O_OBJ):  hfa.h hfa_p.h

%.o:	addtiffo_src/%.cpp
	$(CXX) -c -I../../gcore -I../../port $(ALL_C_FLAGS) \
		addtiffo_src/$*.cpp -o $*.o


hfatest:	hfatest.o $(HFAOBJ)
	$(LD) hfatest.o $(HFAOBJ) ../../port/*.o $(LIBS) -o hfatest

img2tif:	img2tif.cpp imggeotiff.c geoextra.c \
		tif_overview.o rawblockedimage.o $(HFAOBJ)
	$(LD) $(ALL_C_FLAGS) -I../gtiff/libtiff -I../gtiff/libgeotiff \
		img2tif.cpp imggeotiff.c geoextra.c \
		$(HFAOBJ) ../../port/*.o $(LIBS) \
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


install-obj:	$(O_OBJ)

img2tif-dist:
	rm -rf img2tif_dist
	mkdir img2tif_dist
	cp img2tif_build.sh img2tif_dist/build.sh
	cp buildwin.bat img2tif_dist
	mkdir img2tif_dist/port
	cp ../../port/cpl_{config.h.vc,vsil_win32.cpp} img2tif_dist/port
	cp ../../port/cpl_{path.cpp,conv.cpp,conv.h,error.cpp,error.h,port.h,string.cpp,string.h,vsi.h,vsisimple.cpp,config.h,csv.h,csv.cpp,findfile.cpp,multiproc.h,multiproc.cpp,vsil.cpp,vsil_unix_stdio_64.cpp,vsi_mem.cpp,vsi_private.h,dir.cpp} img2tif_dist/port
	mkdir img2tif_dist/libtiff
	cp ../gtiff/libtiff/*.{c,h} img2tif_dist/libtiff
	mkdir img2tif_dist/libgeotiff
	cp ../gtiff/libgeotiff/*.{c,h,inc} img2tif_dist/libgeotiff
	mkdir img2tif_dist/hfa
	cp hfaopen.cpp hfaentry.cpp hfadictionary.cpp hfafield.cpp \
	   hfatype.cpp hfaband.cpp hfacompress.cpp \
	   hfa.h hfa_p.h img2tif_dist/hfa 
	cp BUILD.TXT hfatest.cpp img2tif.cpp imggeotiff.cpp \
	   addtiffo_src/tif_overview.cpp addtiffo_src/rawblocked* \
	   img2tif_dist
