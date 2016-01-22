# $Id: GNUmakefile 16755 2009-04-09 20:38:33Z chaitanya $
#
# Makefile to build dds using GNU Make and GCC.
#

include ../../GDALmake.opt

OBJ	=	ddsdataset.o

CRUNCHINC = -I$(CRUNCHDIR)/include/crunch

CPPFLAGS	:=	$(XTRA_OPT) $(GDAL_INCLUDE) $(CRUNCHINC) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

../o/%.$(OBJ_EXT):
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

all:	$(OBJ:.o=.$(OBJ_EXT))

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
