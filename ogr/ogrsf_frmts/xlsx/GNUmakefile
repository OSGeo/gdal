

include ../../../GDALmake.opt

OBJ	=	ogrxlsxdriver.o ogrxlsxdatasource.o

ifeq ($(HAVE_EXPAT),yes)
CPPFLAGS +=   -DHAVE_EXPAT
endif

CPPFLAGS	:=	-I.. -I../.. -I../mem $(GDAL_INCLUDE) $(EXPAT_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_xlsx.h ../mem/ogr_mem.h