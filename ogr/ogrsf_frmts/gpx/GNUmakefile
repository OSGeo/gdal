

include ../../../GDALmake.opt

OBJ	=	ogrgpxdriver.o ogrgpxdatasource.o ogrgpxlayer.o

ifeq ($(HAVE_EXPAT),yes)
CPPFLAGS +=   -DHAVE_EXPAT
endif

CPPFLAGS	:=	-iquote .. -iquote ../..  $(EXPAT_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_gpx.h

