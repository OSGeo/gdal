GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

ifeq ($(GIF_SETTING),internal)
XTRA_OPT =	-Ilibungif
OBJ	=	egif_lib.o dgif_lib.o gifalloc.o gif_err.o \
		\
		gifdataset.o
else
OBJ	=	gifdataset.o
endif

default:	$(OBJ)

clean:
	rm -f *.o

%.o:	%.c
	$(CC) -c $(GDAL_INCLUDE) $(CFLAGS) $(XTRA_OPT) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CXXFLAGS) $(XTRA_OPT) $*.cpp -o $*.o

%.o:	libungif/%.c
	$(CC) -c $(GDAL_INCLUDE) $(CFLAGS) $(XTRA_OPT) libungif/$*.c -o $*.o


all:	$(OBJ)

install-obj:	$(foreach file,$(OBJ),../o/$(file))

../o/%.o:	%.o
	cp $*.o ../o
