
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	adler32.o compress.o crc32.o deflate.o gzio.o \
		infblock.o infcodes.o inffast.o inflate.o \
		infutil.o inftrees.o trees.o uncompr.o zutil.o 


default:	$(OBJ)

clean:
	rm -f *.o

%.o:	%.c
	$(CC) -c $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(CXXFLAGS) $*.cpp -o $*.o


install-obj:	$(foreach file,$(OBJ),../o/$(file))
