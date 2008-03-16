
include ../../GDALmake.opt

XTRA_OPT =	-I. -DGDALDRIVER

OBJ	=	blxdataset.o blx.o

CPPFLAGS	:=	$(XTRA_OPT) $(GDAL_INCLUDE) $(CPPFLAGS)

default:        $(OBJ)

clean:
	rm -f *.o $(O_OBJ)

install-obj:    $(O_OBJ)
