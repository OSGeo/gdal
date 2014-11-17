

include ../../../GDALmake.opt

OBJ	=	ogrsqlitedatasource.o ogrsqlitelayer.o ogrsqlitedriver.o \
		ogrsqlitetablelayer.o ogrsqliteviewlayer.o ogrsqliteselectlayer.o ogrsqlitesinglefeaturelayer.o \
		ogrsqlitevfs.o ogrsqlitevirtualogr.o ogrsqliteexecutesql.o ogrsqliteapiroutines.o

ifeq ($(HAVE_SPATIALITE),yes)
CPPFLAGS +=  -DHAVE_SPATIALITE
endif

ifeq ($(SPATIALITE_AMALGAMATION),yes)
CPPFLAGS +=  -DSPATIALITE_AMALGAMATION
endif

ifeq ($(SPATIALITE_412_OR_LATER),yes)
CPPFLAGS +=  -DSPATIALITE_412_OR_LATER
endif

ifeq ($(HAVE_PCRE),yes)
CPPFLAGS +=  -DHAVE_PCRE
endif

ifeq ($(SQLITE_HAS_COLUMN_METADATA),yes)
CPPFLAGS +=  -DSQLITE_HAS_COLUMN_METADATA
endif

CPPFLAGS	:=	-I..  $(SQLITE_INC) $(SPATIALITE_INC) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

all: default test_load_virtual_ogr$(EXE)

clean:
	rm -f *.o $(O_OBJ) test_load_virtual_ogr$(EXE)

$(O_OBJ):	ogr_sqlite.h ogrsqlitevirtualogr.h ogrsqliteexecutesql.h ogrsqliteregexp.h ogrsqlitesqlfunctions.h ogrsqliteregexp.cpp ogrsqlitesqlfunctions.cpp

test_load_virtual_ogr$(EXE):	test_load_virtual_ogr.$(OBJ_EXT)
	$(LD) $(LDFLAGS) test_load_virtual_ogr.$(OBJ_EXT) -lsqlite3 -o test_load_virtual_ogr$(EXE)