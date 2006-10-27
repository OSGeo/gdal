
include ../../GDALmake.opt

OBJ	=	wcsdataset.o cpl_http.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) -DHAVE_CURL

default:	$(OBJ)

clean:
	rm -f *.o $(OBJ) $(O_OBJ)

install-obj:	$(O_OBJ)
