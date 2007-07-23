
include ../../GDALmake.opt

OBJ	=	wcsdataset.o httpdriver.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) 

default:	$(OBJ)

clean:
	rm -f *.o $(OBJ) $(O_OBJ)

install-obj:	$(O_OBJ)
