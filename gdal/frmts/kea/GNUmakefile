
include ../../GDALmake.opt

OBJ	=	keaband.o keacopy.o keadataset.o keadriver.o keamaskband.o keaoverview.o kearat.o

CPPFLAGS	:=	 $(KEA_INC) $(CPPFLAGS) -DDO_NOT_USE_DEBUG_BOOL

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(OBJ) $(O_OBJ):	keaband.h keacopy.h keadataset.h keamaskband.h keaoverview.h kearat.h

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
