
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

#OBJ	=	grassdataset.o

ifeq ($(GRASS_SETTING),libgrass)
OBJ	=	grassdataset.o
else
OBJ	=	grass57dataset.o
endif

CPPFLAGS	:=	$(GRASS_INCLUDE) $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ)
