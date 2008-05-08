

include ../../GDALmake.opt

OBJ	=	ceosopen.o ceosdataset.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)

ceostest$(EXE):	ceostest.o
	$(LD) $(LDFLAGS) ceostest.o $(CONFIG_LIBS) -o ceostest$(EXE)

install-obj:	$(O_OBJ)
