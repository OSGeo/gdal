
include ../../GDALmake.opt

ifeq ($(JPEG_SETTING),internal)
JPEG_INCLUDE	=	-I../jpeg/libjpeg
endif

ifeq ($(PCIDSK_SETTING),old)
OBJ	=	pcidskdataset.o pcidsktiledrasterband.o
CPPFLAGS	:=	$(GDAL_INCLUDE) -I../raw $(CPPFLAGS)
$(O_OBJ):	../raw/rawdataset.h gdal_pcidsk.h
else
OBJ	=	pcidskdataset2.o vsi_pcidsk_io.o $(SDKOBJ)
ifeq ($(PCIDSK_SETTING),internal)
CPPFLAGS	:=	-DHAVE_LIBJPEG $(JPEG_INCLUDE) -DPCIDSK_INTERNAL \
	-Isdk $(GDAL_INCLUDE) $(CPPFLAGS)

SDKOBJ 	=	\
	cbandinterleavedchannel.o \
	cpcidskchannel.o \
	cpixelinterleavedchannel.o \
	ctiledchannel.o \
	cpcidskfile.o \
	libjpeg_io.o \
	metadataset_p.o \
	pcidskbuffer.o \
	pcidskcreate.o \
	pcidskexception.o \
	pcidskinterfaces.o \
	pcidskopen.o \
	pcidsk_pubutils.o \
	pcidsk_utils.o \
	sysvirtualfile.o \
	cpcidskgeoref.o \
	cpcidsksegment.o \
	cpcidskvectorsegment.o \
	metadatasegment_p.o \
	sysblockmap.o \
	cpcidskpct.o \
	cpcidskrpcmodel.o \
	cpcidskgcp2segment.o

else
CPPFLAGS	:=	$(GDAL_INCLUDE) $(PCIDSK_INCLUDE) $(CPPFLAGS)
endif
endif

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

../o/%.$(OBJ_EXT):	sdk/%.cpp
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

%.$(OBJ_EXT):	sdk/channel/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

%.$(OBJ_EXT):	sdk/core/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

%.$(OBJ_EXT):	sdk/port/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

%.$(OBJ_EXT):	sdk/segment/%.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS ) $< -o $@

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
