
include ../../GDALmake.opt

OBJ	=	pcidskdataset.o pcidsktiledrasterband.o

CPPFLAGS	:=	$(GDAL_INCLUDE) -I../raw $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

$(O_OBJ):	../raw/rawdataset.h

clean:
	rm -f *.o

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
