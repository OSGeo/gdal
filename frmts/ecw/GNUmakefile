GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	ecwdataset.o

CPPFLAGS	:=	$(GDAL_INCLUDE) -DFRMT_ecw $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o

install-obj:	$(O_OBJ)
