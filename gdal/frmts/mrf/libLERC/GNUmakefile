# $Id$
#
# Makefile to build zlib using GNU Make and GCC.
#
include ../../../GDALmake.opt

OBJ = \
	BitMask.o\
	BitMask2.o\
	BitStuffer.o\
	BitStuffer2.o\
	CntZImage.o\
	Huffman.o\
	RLE.o\
	Lerc2.o

LIBLERC = libLERC.o

CPPFLAGS	:=	$(CPPFLAGS)

default:	$(OBJ) $(LIBLERC)
	
$(LIBLERC):	$(OBJ)
	$(AR) r $@ $^
	cp $@ ..

install-obj:	$(OBJ)
	$(AR) r ../../o/$(LIBLERC) $^

clean:
	rm -f $(OBJ)
