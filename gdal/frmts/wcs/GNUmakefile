
include ../../GDALmake.opt

OBJ	=	wcsdataset.o cpl_http.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) -DHAVE_CURL $(CURL_INC)

default:	$(OBJ)

clean:
	rm -f *.o $(OBJ) $(O_OBJ)

install-obj:	$(O_OBJ)
