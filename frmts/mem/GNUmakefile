

include ../../GDALmake.opt

OBJ	=	memdataset.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ)

install:
	$(INSTALL_DATA) memdataset.h $(INST_INCLUDE)
