
include ../../GDALmake.opt

OBJ	=	sdedataset.o sdeerror.o sderasterband.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(SDE_INC) -DFRMT_sde $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
