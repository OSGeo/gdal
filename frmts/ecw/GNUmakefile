
include ../../GDALmake.opt

OBJ	=	ecwdataset.o ecwcreatecopy.o jp2userbox.o

CPPFLAGS	:=	$(GDAL_INCLUDE) -DFRMT_ecw $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ)
