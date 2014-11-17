

include ../../GDALmake.opt

OBJ	=	dted_api.o dteddataset.o dted_create.o dted_ptstream.o



default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

dted_test$(EXE):	dted_test.$(OBJ_EXT)
	$(LD) $(LDFLAGS) dted_test.$(OBJ_EXT) $(CONFIG_LIBS) -o dted_test$(EXE)
