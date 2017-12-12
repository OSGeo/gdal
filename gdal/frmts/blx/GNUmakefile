
include ../../GDALmake.opt

XTRA_OPT =	-DGDALDRIVER

OBJ	=	blxdataset.o blx.o

CPPFLAGS	:=	$(XTRA_OPT)  $(CPPFLAGS)

default:        $(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:    $(O_OBJ:.o=.$(OBJ_EXT))
