
include ../../GDALmake.opt

OBJ	=	coasp_dataset.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) 

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
