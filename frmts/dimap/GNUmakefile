include ../../GDALmake.opt

OBJ	=	dimapdataset.o

CPPFLAGS        :=      -iquote ../vrt  $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

$(OBJ) $(O_OBJ):	 ../vrt/vrtdataset.h

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

install:
	$(INSTALL_DATA) memdataset.h $(DESTDIR)$(INST_INCLUDE)
