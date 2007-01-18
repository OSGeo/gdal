
include ../../GDALmake.opt

OBJ	=	sdedataset.o

CPPFLAGS	:=	$(GDAL_INCLUDE) -DFRMT_sde $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ)
