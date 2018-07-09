# $Id$
#
# Makefile to build zlib using GNU Make and GCC.
#
include ../../../GDALmake.opt

OBJ = \
	BitMaskV1.o\
	BitStufferV1.o\
	CntZImage.o

O_OBJ   =       $(foreach file,$(OBJ),../../o/$(file))

default:	$(OBJ:.o=.$(OBJ_EXT))

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f $(O_OBJ) *.o *.lo $(O_OBJ:.o=.$(OBJ_EXT))

../../o/%.$(OBJ_EXT):   %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -I../../port $< -o $@

