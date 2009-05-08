
OBJ	=	xpmdataset.o

include ../../GDALmake.opt

XTRAOPTS = -I../mem

CPPFLAGS	:=	$(GDAL_INCLUDE) $(XTRAOPTS) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
