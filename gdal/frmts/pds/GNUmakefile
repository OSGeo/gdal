
include ../../GDALmake.opt

OBJ	=	pdsdataset.o isis2dataset.o

CPPFLAGS	:=	-I../raw $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ)

