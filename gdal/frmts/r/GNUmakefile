
include ../../GDALmake.opt

OBJ	=	rdataset.o rcreatecopy.o



default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
