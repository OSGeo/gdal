GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	netcdfdataset.o gmtdataset.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) $(XTRA_OPT) 

default:	$(OBJ)

clean:
	rm -f *.o

install-obj:	$(O_OBJ)
