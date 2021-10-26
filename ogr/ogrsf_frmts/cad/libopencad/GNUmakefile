
include ../../../../GDALmake.opt

SUBDIRS-yes :=	dwg

OBJ := \
	cadfile.o \
	cadcolors.o \
	cadlayer.o \
	cadclasses.o \
	cadfileio.o \
	cadobjects.o \
	cadtables.o \
	cadfilestreamio.o \
	cadgeometry.o \
	cadheader.o \
	opencad.o \
	caddictionary.o

O_OBJ := $(foreach file,$(OBJ),../../o/$(file))

CPPFLAGS	:=	-iquote . -iquote .. -iquote ../.. $(CPPFLAGS)

default:	$(foreach d,$(SUBDIRS-yes),$(d)-target) $(O_OBJ:.o=.$(OBJ_EXT))

../../o/%.$(OBJ_EXT):	%.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

clean: $(foreach d,$(SUBDIRS-yes),$(d)-clean)
	rm -f *.o $(O_OBJ)
	rm -f *~
