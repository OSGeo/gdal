
GDAL_ROOT	=	../..

OBJ	=	sar_ceosdataset.o \
		ceosrecipe.o ceossar.o ceos.o link.o

include $(GDAL_ROOT)/GDALmake.opt

XTRAOPTS =	-I../raw

CPPFLAGS	+=	$(GDAL_INCLUDE) $(XTRAOPTS)

default:	$(OBJ)

clean:
	rm -f *.o

install-obj:	$(O_OBJ)
