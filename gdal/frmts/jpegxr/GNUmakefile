
OBJ	=	jpegxrdataset.o

include ../../GDALmake.opt

CPPFLAGS	:=	$(JXRLIB_INCLUDE) $(JXRLIB_FLAGS) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
