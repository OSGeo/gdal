
include ../../GDALmake.opt

OBJ	=	zarrdataset.o

CPPFLAGS	:=	$(ZARR_INCLUDE) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
