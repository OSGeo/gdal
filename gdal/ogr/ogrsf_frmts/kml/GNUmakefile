

include ../../../GDALmake.opt

CORE_OBJ =	ogr2kmlgeometry.o

OGR_OBJ =	ogrkmldriver.o ogrkmldatasource.o ogrkmllayer.o

OBJ =	$(CORE_OBJ) $(OGR_OBJ)

ifeq ($(HAVE_EXPAT),yes)
OGR_OBJ  +=   kml.o kmlnode.o kmlvector.o
CPPFLAGS +=   -DHAVE_EXPAT
endif

CPPFLAGS := -I. $(CPPFLAGS) $(EXPAT_INCLUDE)

#CFLAGS	:=	$(filter-out -Wall,$(CFLAGS))

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o kmlview $(O_OBJ)

$(O_OBJ):	kml.h kmlnode.h kmlutility.h kmlvector.h ogr_kml.h
