
# Comment out to skip use of libjpeg
JPEG_FLAGS = -DHAVE_LIBJPEG
JPEG_LDFLAGS = -ljpeg

OBJ = \
	channel/cbandinterleavedchannel.o \
	channel/cpcidskchannel.o \
	channel/cpixelinterleavedchannel.o \
	channel/ctiledchannel.o \
	core/cpcidskfile.o \
	core/libjpeg_io.o \
	core/metadataset_p.o \
	core/pcidskbuffer.o \
	core/pcidskcreate.o \
	core/pcidskexception.o \
	core/pcidskinterfaces.o \
	core/pcidskopen.o \
	core/pcidsk_pubutils.o \
	core/pcidsk_utils.o \
	core/sysvirtualfile.o \
	port/io_stdio.o \
	port/pthread_mutex.o \
	segment/cpcidskgeoref.o \
	segment/cpcidskpct.o \
	segment/cpcidsksegment.o \
	segment/cpcidskvectorsegment.o \
	segment/metadatasegment_p.o \
	segment/sysblockmap.o \
	segment/cpcidskrpcmodel.o


CXXFLAGS = -g -Wall -fPIC -I. $(JPEG_FLAGS)

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
	(cd port; $(MAKE))

check:	default	
	(cd ../tests; $(MAKE) check)

$(OBJ):	pcidsk.h pcidsk_config.h 

%.o:	%.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

docs:
	doxygen

