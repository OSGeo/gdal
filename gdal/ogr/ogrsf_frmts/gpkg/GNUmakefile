
include ../../../GDALmake.opt

OBJ	= ogrgeopackagedriver.o ogrgeopackagedatasource.o ogrgeopackagelayer.o \
	ogrgeopackagetablelayer.o ogrgeopackageselectlayer.o ogrgeopackageutility.o \
	gdalgeopackagerasterband.o

ifeq ($(SPATIALITE_412_OR_LATER),yes)
CPPFLAGS +=  -DSPATIALITE_412_OR_LATER
endif

ifeq ($(SQLITE_HAS_COLUMN_METADATA),yes)
CPPFLAGS +=  -DSQLITE_HAS_COLUMN_METADATA
endif

ifeq ($(HAVE_RASTERLITE2),yes)
CPPFLAGS +=  -DHAVE_RASTERLITE2 $(RASTERLITE2_CFLAGS)
endif

CPPFLAGS := -iquote .. -iquote ../sqlite -iquote ../../../frmts/mem -iquote ../../../frmts $(SQLITE_INC) $(CPPFLAGS)

default: $(O_OBJ:.o=.$(OBJ_EXT))


clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ): ogr_geopackage.h ogrgeopackageutility.h gpkgmbtilescommon.h ../sqlite/ogr_sqlite.h


