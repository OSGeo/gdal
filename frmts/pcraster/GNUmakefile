GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	pcrasterdataset.o pcrastermisc.o pcrasterrasterband.o pcrasterutil.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)
	rm -fR html

install-obj:	$(O_OBJ)

docs:
	doxygen doxygen.cfg

ctags:
	ctags *cpp *h
