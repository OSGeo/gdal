
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	aaigriddataset.o

CPPFLAGS	=	$(GDAL_INCLUDE) $(XTRA_OPT)

default:	$(OBJ)

clean:
	rm -f *.o

all:	$(OBJ)

install-obj:	$(foreach file,$(OBJ),../o/$(file))
