
include ../../GDALmake.opt

OBJ	=	rasterlitedataset.o rasterlitecreatecopy.o rasterliteoverviews.o

CPPFLAGS	:=	 $(CPPFLAGS) -I../../ogr

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
