
# Comment out to skip use of libjpeg
JPEG_FLAGS = -DHAVE_LIBJPEG
JPEG_LDFLAGS = -ljpeg

OBJ = \
	blockdir/asciitiledir.o \
	blockdir/asciitilelayer.o \
	blockdir/binarytiledir.o \
	blockdir/binarytilelayer.o \
	blockdir/blockdir.o \
	blockdir/blockfile.o \
	blockdir/blocklayer.o \
	blockdir/blocktiledir.o \
	blockdir/blocktilelayer.o \
	channel/cbandinterleavedchannel.o \
	channel/cexternalchannel.o \
	channel/cpcidskchannel.o \
	channel/cpixelinterleavedchannel.o \
	channel/ctiledchannel.o \
	core/clinksegment.o \
	core/cpcidskblockfile.o \
	core/cpcidskfile.o \
	core/edb_pcidsk.o \
	core/libjpeg_io.o \
	core/libkakadu_io.o \
	core/metadataset_p.o \
	core/pcidskbuffer.o \
	core/pcidskcreate.o \
	core/pcidskexception.o \
	core/pcidskinterfaces.o \
	core/pcidskopen.o \
	core/pcidsk_pubutils.o \
	core/pcidsk_raster.o \
	core/pcidsk_scanint.o \
	core/pcidsk_utils.o \
	port/io_stdio.o \
	port/pthread_mutex.o \
	segment/cpcidsk_array.o \
	segment/cpcidskbinarysegment.o \
	segment/cpcidskbitmap.o \
	segment/cpcidskblut.o \
	segment/cpcidskbpct.o \
	segment/cpcidskephemerissegment.o \
	segment/cpcidskgcp2segment.o \
	segment/cpcidskgeoref.o \
	segment/cpcidsklut.o \
	segment/cpcidskpct.o \
	segment/cpcidskpolymodel.o \
	segment/cpcidskrpcmodel.o \
	segment/cpcidsksegment.o \
	segment/cpcidsk_tex.o \
	segment/cpcidsktoutinmodel.o \
	segment/cpcidskvectorsegment_consistencycheck.o \
	segment/cpcidskvectorsegment.o \
	segment/metadatasegment_p.o \
	segment/systiledir.o \
	segment/vecsegdataindex.o \
	segment/vecsegheader.o

CXXFLAGS = -g -O0 -Wall -fPIC -iquote . $(JPEG_FLAGS)

default:	pcidsk.a libpcidsk.so

pcidsk.a: $(OBJ)
	ar r pcidsk.a $(OBJ)

libpcidsk.so: $(OBJ)
	g++ -fPIC $(JPEG_LDFLAGS) -ldl -shared -o libpcidsk.so $(OBJ)

clean:
	rm -f $(OBJ)
	-rm -f *.o
	-rm -f *.a
	-rm -f *.so

core_comp:
	(cd core; $(MAKE))
	(cd channel; $(MAKE))
	(cd segment; $(MAKE))
	(cd blockdir; $(MAKE))
	(cd port; $(MAKE))

check:	default	
	(cd ../tests; $(MAKE) check)

$(OBJ):	pcidsk.h pcidsk_config.h 

%.o:	%.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

docs:
	doxygen

