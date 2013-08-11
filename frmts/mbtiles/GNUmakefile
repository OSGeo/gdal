
include ../../GDALmake.opt

OBJ	=	mbtilesdataset.o

ifeq ($(LIBZ_SETTING),internal)
  XTRA_OPT :=   $(XTRA_OPT) -I../zlib
endif

CPPFLAGS	:=	$(JSON_INCLUDE) $(XTRA_OPT) $(GDAL_INCLUDE) $(CPPFLAGS) -I../../ogr

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
