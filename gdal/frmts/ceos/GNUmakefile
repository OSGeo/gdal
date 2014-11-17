

include ../../GDALmake.opt

OBJ	=	ceosopen.o ceosdataset.o



default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

ceostest$(EXE):	ceostest.$(OBJ_EXT)
	$(LD) $(LDFLAGS) ceostest.$(OBJ_EXT) $(CONFIG_LIBS) -o ceostest$(EXE)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
