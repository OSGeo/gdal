GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	ecwdataset.o ecwcreatecopy.o

CPPFLAGS	:=	$(GDAL_INCLUDE) -DFRMT_ecw $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ)
