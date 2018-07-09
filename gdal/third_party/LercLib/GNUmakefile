include ../../GDALmake.opt

OBJ = \
	BitMask.o \
	BitStuffer2.o \
	Huffman.o \
	Lerc2.o \
	Lerc_c_api_impl.o \
	Lerc.o \
	RLE.o \

default:        $(OBJ:.o=.$(OBJ_EXT))

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o
