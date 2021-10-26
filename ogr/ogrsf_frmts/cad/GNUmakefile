
include ../../../GDALmake.opt

SUBDIRS-yes := libopencad

OBJ	:= \
	ogrcaddriver.o \
	gdalcaddataset.o \
	ogrcadlayer.o \
	vsilfileio.o

CPPFLAGS	:=	-iquote . -iquote .. -iquote ../.. -iquote libopencad $(CPPFLAGS)

default:	$(foreach d,$(SUBDIRS-yes),$(d)-target) $(O_OBJ:.o=.$(OBJ_EXT))

clean: $(foreach d,$(SUBDIRS-yes),$(d)-clean)
	rm -f *.o $(O_OBJ)
	rm -f *~

$(O_OBJ):	ogr_cad.h
