

include ../../../GDALmake.opt

OBJ	=	ogrsqlitedatasource.o ogrsqlitelayer.o ogrsqlitedriver.o \
		ogrsqlitetablelayer.o ogrsqliteselectlayer.o ogrsqlitesinglefeaturelayer.o

ifeq ($(HAVE_SPATIALITE),yes)
CPPFLAGS +=  -DHAVE_SPATIALITE
endif

CPPFLAGS	:=	-I.. $(GDAL_INCLUDE) $(SQLITE_INC) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_sqlite.h
