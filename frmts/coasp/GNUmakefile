
include ../../GDALmake.opt

OBJ	=	coasp_dataset.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) 

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ)
