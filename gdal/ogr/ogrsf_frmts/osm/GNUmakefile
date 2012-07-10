

include ../../../GDALmake.opt

OBJ =	ogrosmdriver.o ogrosmdatasource.o ogrosmlayer.o osm_parser.o

CPPFLAGS :=	-I.. -I../.. -I../sqlite $(GDAL_INCLUDE) $(EXPAT_INCLUDE) $(SQLITE_INC) $(CPPFLAGS)

ifeq ($(HAVE_EXPAT),yes)
CPPFLAGS +=  -DHAVE_EXPAT
endif

ifeq ($(LIBZ_SETTING),internal)
CPPFLAGS +=      -I../../../frmts/zlib
endif


default:	$(O_OBJ:.o=.$(OBJ_EXT))

all: default osm2osm$(EXE)

clean:
	rm -f *.o $(O_OBJ) osm2osm$(EXE)

$(O_OBJ):	ogr_osm.h gpb.h osm_parser.h

osm2osm$(EXE):	osm2osm.$(OBJ_EXT)
	$(LD) $(LDFLAGS) osm2osm.$(OBJ_EXT) $(CONFIG_LIBS) -o osm2osm$(EXE)
