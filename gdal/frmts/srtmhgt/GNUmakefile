
include ../../GDALmake.opt

OBJ	= srtmhgtdataset.o

CPPFLAGS	:=	$(XTRA_OPT) $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)

../o/%.o:
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

all:	$(OBJ)

install-obj:	$(O_OBJ)
