
include ../../GDALmake.opt

OBJ    =       msgndataset.o msg_basic_types.o msg_reader_core.o

CPPFLAGS :=	 $(CPPFLAGS) -I. -DGDAL_SUPPORT

default:       $(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
       
install-obj:   $(O_OBJ:.o=.$(OBJ_EXT))
