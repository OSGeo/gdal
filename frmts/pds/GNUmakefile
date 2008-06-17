
include ../../GDALmake.opt

OBJ	=	pdsdataset.o isis2dataset.o isis3dataset.o nasakeywordhandler.o

CPPFLAGS	:=	-I../raw $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

$(O_OBJ):       nasakeywordhandler.h ../raw/rawdataset.h

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ)

