
include ../../GDALmake.opt

OBJ	= srtmhgtdataset.o

CPPFLAGS	:=	$(XTRA_OPT) $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

../o/%.$(OBJ_EXT):
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

all:	$(OBJ:.o=.$(OBJ_EXT))

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
