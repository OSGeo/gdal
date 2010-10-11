

include ../../GDALmake.opt

OBJ	=	kmlsuperoverlaydataset.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ:.o=.$(OBJ_EXT))

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

