GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	ecwdataset.o

CPPFLAGS	=	$(GDAL_INCLUDE) -DFRMT_ecw

default:	$(OBJ)

clean:
	rm -f *.o

install-obj:	$(foreach file,$(OBJ),../o/$(file))
