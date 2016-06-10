
include ../../../../GDALmake.opt

SUBDIRS-yes :=	dwg

OBJ = \
	cadfile.o \
	cadlayer.o \
	cadclasses.o \
	cadfileio.o \
	cadobjects.o \
	cadtables.o \
	cadfilestreamio.o \
	cadgeometry.o \
	cadheader.o \
	opencad.o 

O_OBJ = $(foreach file,$(OBJ),../../o/$(file))

CPPFLAGS	:=	-I.. -I../.. $(CPPFLAGS) -std=c++11

default:	$(foreach d,$(SUBDIRS-yes),$(d)-target) $(O_OBJ:.o=.$(OBJ_EXT))

../../o/%.$(OBJ_EXT):	%.cpp
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

clean:
	$(foreach d,$(SUBDIRS-yes),$(d)-clean)
	rm -f *.o $(O_OBJ)
	rm -f *~

$(O_OBJ):	opencad_api.h

