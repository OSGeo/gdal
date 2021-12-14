
include ../../GDALmake.opt

ifneq ($(JPEG_SETTING),no)
JPEG_FLAGS = -DHAVE_LIBJPEG
endif

ifeq ($(JPEG_SETTING),internal)
JPEG_INCLUDE	=	-I../jpeg/libjpeg
ifeq ($(RENAME_INTERNAL_LIBJPEG_SYMBOLS),yes)
JPEG_FLAGS	:=	-DRENAME_INTERNAL_LIBJPEG_SYMBOLS $(JPEG_FLAGS)
endif
endif

ifeq ($(PCIDSK_SETTING),no)
# Do nothing.
else
OBJ	=	pcidskdataset2.o ogrpcidsklayer.o vsi_pcidsk_io.o gdal_edb.o $(SDKOBJ)
ifeq ($(PCIDSK_SETTING),internal)
CPPFLAGS	:=	$(JPEG_FLAGS) $(JPEG_INCLUDE) -DPCIDSK_INTERNAL \
	-Isdk  $(CPPFLAGS)

SDKOBJ 	=	\
	asciitiledir.o \
	asciitilelayer.o \
	binarytiledir.o \
	binarytilelayer.o \
	blockdir.o \
	blockfile.o \
	blocklayer.o \
	blocktiledir.o \
	blocktilelayer.o \
	cbandinterleavedchannel.o \
	cpcidskchannel.o \
	cpixelinterleavedchannel.o \
	ctiledchannel.o \
	cexternalchannel.o \
	cpcidskfile.o \
	cpcidskblockfile.o \
	libjpeg_io.o \
	edb_pcidsk.o \
	metadataset_p.o \
	pcidskbuffer.o \
	pcidskcreate.o \
	pcidskexception.o \
	pcidskinterfaces.o \
	pcidskopen.o \
	pcidsk_pubutils.o \
	pcidsk_raster.o \
	pcidsk_scanint.o \
	pcidsk_utils.o \
	cpcidskgeoref.o \
	cpcidsksegment.o \
	cpcidskvectorsegment.o \
	cpcidskvectorsegment_consistencycheck.o \
	systiledir.o \
	vecsegheader.o \
	vecsegdataindex.o \
	metadatasegment_p.o \
	cpcidsklut.o \
	cpcidskblut.o \
	cpcidskpct.o \
	cpcidskbpct.o \
	cpcidskpolymodel.o \
	cpcidskrpcmodel.o \
	cpcidskgcp2segment.o \
	cpcidskbitmap.o \
	cpcidsk_tex.o \
	clinksegment.o \
	cpcidsktoutinmodel.o \
	cpcidskbinarysegment.o \
	cpcidsk_array.o \
	cpcidskephemerissegment.o

else
CPPFLAGS	:=	 $(PCIDSK_INCLUDE) $(CPPFLAGS)
endif
endif

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

../o/%.$(OBJ_EXT):	sdk/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

../o/%.$(OBJ_EXT):	sdk/blockdir/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

../o/%.$(OBJ_EXT):	sdk/channel/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

../o/%.$(OBJ_EXT):	sdk/core/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

../o/%.$(OBJ_EXT):	sdk/port/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

../o/%.$(OBJ_EXT):	sdk/segment/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

%.$(OBJ_EXT):	sdk/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

%.$(OBJ_EXT):	sdk/blockdir/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

%.$(OBJ_EXT):	sdk/channel/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

%.$(OBJ_EXT):	sdk/core/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

%.$(OBJ_EXT):	sdk/port/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

%.$(OBJ_EXT):	sdk/segment/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

import:
	cp ~/pcidsk/src/*.h sdk
	cp ~/pcidsk/src/raster/*.h* sdk/raster
	cp ~/pcidsk/src/blockdir/*.cpp sdk/blockdir
	cp ~/pcidsk/src/blockdir/*.h sdk/blockdir
	cp ~/pcidsk/src/channel/*.cpp sdk/channel
	cp ~/pcidsk/src/channel/*.h sdk/channel
	cp ~/pcidsk/src/segment/*.cpp sdk/segment
	cp ~/pcidsk/src/segment/*.h sdk/segment
	cp ~/pcidsk/src/core/*.cpp sdk/core
	cp ~/pcidsk/src/core/*.h sdk/core
	cp ~/pcidsk/src/port/*.cpp sdk/port
