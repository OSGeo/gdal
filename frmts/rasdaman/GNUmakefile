
include ../../GDALmake.opt

OBJ	=	rasdamandataset.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) -I/home/costea/work/os/rasdaman/include 

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
