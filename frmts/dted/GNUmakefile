

include ../../GDALmake.opt

OBJ	=	dted_api.o dteddataset.o dted_create.o dted_ptstream.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o

install-obj:	$(O_OBJ)

dted_test$(EXE):	dted_test.o
	$(LD) $(LDFLAGS) dted_test.o $(CONFIG_LIBS) -o dted_test$(EXE)
