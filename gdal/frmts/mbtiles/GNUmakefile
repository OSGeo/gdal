
include ../../GDALmake.opt

OBJ	=	mbtilesdataset.o

ifeq ($(LIBZ_SETTING),internal)
  XTRA_OPT :=   $(XTRA_OPT) -I../zlib
endif

CPPFLAGS	:=	$(XTRA_OPT) $(GDAL_INCLUDE) $(CPPFLAGS) -I../../ogr -I../../ogr/ogrsf_frmts/geojson

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
