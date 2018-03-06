
include ../../GDALmake.opt

OBJ	=	pdsdataset.o isis2dataset.o isis3dataset.o vicardataset.o nasakeywordhandler.o vicarkeywordhandler.o pds4dataset.o

CPPFLAGS	:=	-I../raw -I../vrt -I../../ogr/ogrsf_frmts/geojson $(JSON_INCLUDE) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

$(O_OBJ):       nasakeywordhandler.h vicarkeywordhandler.h ../raw/rawdataset.h ../vrt/vrtdataset.h

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

