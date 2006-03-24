
include ../../GDALmake.opt

ifeq ($(LIBZ_SETTING),internal)
XTRA_OPT =	-I../zlib
else
XTRA_OPT =
endif

OBJ	=	rikdataset.o

CPPFLAGS	:=	$(XTRA_OPT) $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ)
