
include ../../GDALmake.opt

OBJ	=	usgsdemdataset.o usgsdem_create.o

CPPFLAGS	:=	$(GDAL_INCLUDE) -I../../alg $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o

install-obj:	$(O_OBJ)
