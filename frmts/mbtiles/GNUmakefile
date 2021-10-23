
include ../../GDALmake.opt

OBJ	=	mbtilesdataset.o

ifeq ($(LIBZ_SETTING),internal)
  XTRA_OPT :=   $(XTRA_OPT) -I../zlib
endif

CPPFLAGS	:=	-iquote ../../ogr/ogrsf_frmts/geojson -iquote ../../ogr/ogrsf_frmts/mvt $(JSON_INCLUDE) $(XTRA_OPT)  $(CPPFLAGS) -iquote ../../ogr -iquote ../../ogr/ogrsf_frmts/gpkg -iquote ../../ogr/ogrsf_frmts/sqlite $(SQLITE_INC) -DHAVE_SQLITE

ifeq ($(HAVE_GEOS),yes)
CPPFLAGS 	:=	-DHAVE_GEOS=1 $(CPPFLAGS)
endif

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

$(O_OBJ): ../../ogr/ogrsf_frmts/gpkg/gpkgmbtilescommon.h
