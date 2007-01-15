
include ../../GDALmake.opt

OBJ	=	rmfdataset.o rmflzw.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o

install-obj:	$(O_OBJ)
