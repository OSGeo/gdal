
include ../../GDALmake.opt

OBJ	=	aaigriddataset.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) $(XTRA_OPT)

default:	$(OBJ)

clean:
	rm -f *.o

all:	$(OBJ)

install-obj:	$(O_OBJ)
