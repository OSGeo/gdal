GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	png.o pngerror.o pnggccrd.o pngget.o pngmem.o \
		pngpread.o pngread.o pngrio.o pngrtran.o pngrutil.o \
		pngset.o pngtrans.o pngvcrd.o pngwio.o pngwrite.o \
		pngwtran.o pngwutil.o \
		\
		pngdataset.o

XTRA_OPT	=	-I../zlib

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
