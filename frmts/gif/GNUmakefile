GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

ifeq ($(GIF_SETTING),internal)
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
	$(CC) -c $(CFLAGS) $(GDAL_INCLUDE) $(XTRA_OPT) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(CXXFLAGS) $(GDAL_INCLUDE) $(XTRA_OPT) $*.cpp -o $*.o


all:	$(OBJ)

install-obj:	$(foreach file,$(OBJ),../o/$(file))

../o/%.o:	%.o
	cp $*.o ../o
