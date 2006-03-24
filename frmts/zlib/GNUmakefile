

include ../../GDALmake.opt

OBJ	=	adler32.o compress.o crc32.o deflate.o gzio.o \
		infblock.o infcodes.o inffast.o inflate.o \
		infutil.o inftrees.o trees.o uncompr.o zutil.o 


default:	$(OBJ)

clean:
	rm -f *.o

install-obj:	$(O_OBJ)
