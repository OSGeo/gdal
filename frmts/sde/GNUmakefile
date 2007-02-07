
include ../../GDALmake.opt

OBJ	=	sdedataset.o sdeerror.o sderasterband.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(SDE_INC) -DFRMT_sde $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ)
