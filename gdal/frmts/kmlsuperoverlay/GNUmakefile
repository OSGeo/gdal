
include ../../GDALmake.opt

OBJ	=	kmlsuperoverlaydataset.o

CPPFLAGS	:=	$(GDAL_INCLUDE) -I/usr/include/minizip $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ)
