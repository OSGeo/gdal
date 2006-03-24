
include ../../GDALmake.opt

OBJ	=	pcidskdataset.o pcidsktiledrasterband.o

CPPFLAGS	:=	$(GDAL_INCLUDE) -I../raw $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o

install-obj:	$(O_OBJ)
