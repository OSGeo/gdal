

include ../../GDALmake.opt

OBJ	=	memdataset.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

install:
	$(INSTALL_DATA) memdataset.h $(DESTDIR)$(INST_INCLUDE)
