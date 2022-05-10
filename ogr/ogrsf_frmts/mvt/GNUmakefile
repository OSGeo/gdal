

include ../../../GDALmake.opt

OBJ	=	ogrmvtdataset.o mvtutils.o mvt_tile.o

CPPFLAGS	:=	-iquote .. -iquote ../.. -iquote ../osm $(CPPFLAGS)

ifeq ($(HAVE_SQLITE),yes)
CPPFLAGS :=	$(CPPFLAGS) -DHAVE_SQLITE $(SQLITE_INC)
endif

ifeq ($(HAVE_GEOS),yes)
CPPFLAGS 	:=	-DHAVE_GEOS=1 $(GEOS_CFLAGS) $(CPPFLAGS)
endif

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ) mvt_tile_test$(EXE)

mvt_tile_test$(EXE): mvt_tile_test.$(OBJ_EXT)
	$(LD) $(LDFLAGS) mvt_tile_test.$(OBJ_EXT) $(CONFIG_LIBS) -o mvt_tile_test$(EXE)

