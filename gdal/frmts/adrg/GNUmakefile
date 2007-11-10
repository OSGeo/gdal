

include ../../GDALmake.opt

OBJ	=	adrgdataset.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) -I../iso8211

default:	$(OBJ)

clean:
	rm -f *.o

install-obj:	$(O_OBJ)
