
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	ceosopen.o ceosdataset.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o

ceostest:	ceostest.o ceosopen.o
	$(CXX) ceostest.o ceosopen.o $(GDAL_ROOT)/port/*.o $(LIBS) -o ceostest

install-obj:	$(O_OBJ)
