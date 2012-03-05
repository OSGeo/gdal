

SUBDIRS-yes     := iom

include ../../../GDALmake.opt

OBJ	=	ogrili1driver.o ogrili1datasource.o ogrili1layer.o \
                ogrili2driver.o ogrili2datasource.o ogrili2layer.o \
                ili1reader.o ili2reader.o ili2handler.o \
                iomhelper.o ilihelper.o

ifeq ($(HAVE_GEOS),yes)
CPPFLAGS 	:=	-DHAVE_GEOS=1 $(GEOS_CFLAGS) $(CPPFLAGS)
endif

CPPFLAGS :=	-I.. -I../.. $(GDAL_INCLUDE) $(XERCES_INCLUDE) $(CPPFLAGS)

default:	$(foreach d,$(SUBDIRS-yes),$(d)-target)	$(O_OBJ:.o=.$(OBJ_EXT))

clean: $(foreach d,$(SUBDIRS-yes),$(d)-clean)
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_ili1.h ogr_ili2.h iomhelper.h ili1reader.h ili1readerp.h ili2reader.h ili2readerp.h
