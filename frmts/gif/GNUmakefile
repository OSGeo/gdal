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
	$(CC) -c $(XTRA_OPT) $(GDAL_INCLUDE) $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(XTRA_OPT) $(GDAL_INCLUDE) $(CXXFLAGS) $*.cpp -o $*.o

%.o:	libungif/%.c
	$(CC) -c $(XTRA_OPT) $(GDAL_INCLUDE) $(CFLAGS) libungif/$*.c -o $*.o


all:	$(OBJ)

install-obj:	$(foreach file,$(OBJ),../o/$(file))
