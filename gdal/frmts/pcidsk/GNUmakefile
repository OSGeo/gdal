
include ../../GDALmake.opt

OBJ	=	pcidskdataset.o pcidsktiledrasterband.o

CPPFLAGS	:=	$(GDAL_INCLUDE) -I../raw $(CPPFLAGS)

default:	$(OBJ)

$(O_OBJ):	../raw/rawdataset.h

clean:
	rm -f *.o

install-obj:	$(O_OBJ)
