

OBJ	=	sar_ceosdataset.o \
		ceosrecipe.o ceossar.o ceos.o link.o

include ../../GDALmake.opt


CPPFLAGS	:=	-I../raw $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ)
