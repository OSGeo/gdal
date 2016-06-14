
include ../../GDALmake.opt

OBJ	=	jpeg2000dataset.o jpeg2000_vsil_io.o

CPPFLAGS	:=	 $(JASPER_FLAGS) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f $(OBJ) $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
