
include ../../GDALmake.opt

OBJ	=	pdsdataset.o isis2dataset.o isis3dataset.o vicardataset.o \
		vicarkeywordhandler.o \
		pds4dataset.o pds4vector.o

ifeq ($(GEOTIFF_SETTING),internal)
GEOTIFF_INCLUDE	=	-iquote ../gtiff/libgeotiff
ifeq ($(RENAME_INTERNAL_LIBGEOTIFF_SYMBOLS),yes)
CPPFLAGS	:=	-DRENAME_INTERNAL_LIBGEOTIFF_SYMBOLS $(CPPFLAGS)
endif
endif

LIBTIFF_OPTS	=
ifeq ($(TIFF_SETTING),internal)
LIBTIFF_OPTS	:=	-iquote ../gtiff/libtiff $(LIBTIFF_OPTS)
ifeq ($(RENAME_INTERNAL_LIBTIFF_SYMBOLS),yes)
LIBTIFF_OPTS	:=	-DRENAME_INTERNAL_LIBTIFF_SYMBOLS $(LIBTIFF_OPTS)
endif
endif

CPPFLAGS	:=	-iquote ../vrt -iquote ../../ogr/ogrsf_frmts/geojson -iquote ../../ogr/ogrsf_frmts/generic -iquote ../../ogr/ogrsf_frmts/vrt $(JSON_INCLUDE) $(GEOTIFF_INCLUDE) $(LIBTIFF_OPTS)  -iquote ../gtiff $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

$(O_OBJ):       ../../gcore/nasakeywordhandler.h vicarkeywordhandler.h pds4dataset.h ../vrt/vrtdataset.h ../../gcore/rawdataset.h

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

