
include ../../GDALmake.opt

OBJ	=	tildataset.o

CPPFLAGS	:=	-iquote ../vrt  $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

$(OBJ) $(O_OBJ):	 ../vrt/vrtdataset.h

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
