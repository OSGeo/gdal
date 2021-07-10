
include ../../GDALmake.opt

OBJ	=	pdsdataset.o isis2dataset.o isis3dataset.o vicardataset.o \
		vicarkeywordhandler.o \
		pds4dataset.o pds4vector.o

CPPFLAGS	:=	-iquote ../vrt -iquote ../../ogr/ogrsf_frmts/geojson -iquote ../../ogr/ogrsf_frmts/generic -iquote ../../ogr/ogrsf_frmts/vrt $(JSON_INCLUDE) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

$(O_OBJ):       ../../gcore/nasakeywordhandler.h vicarkeywordhandler.h pds4dataset.h ../vrt/vrtdataset.h ../../gcore/rawdataset.h

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

