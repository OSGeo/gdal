

include ../../../GDALmake.opt

OBJ	=	ogrmongodbdriver.o

CPPFLAGS	:=	-iquote ../generic  $(MONGODB_INC) $(CPPFLAGS)
# Does not even solve runtime problems
#-DDO_NOT_USE_DEBUG_BOOL

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
