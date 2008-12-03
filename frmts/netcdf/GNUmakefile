
include ../../GDALmake.opt

OBJ	=	netcdfdataset.o gmtdataset.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) $(XTRA_OPT) 

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
