
include ../../GDALmake.opt

ifeq ($(LIBZ_SETTING),internal)
XTRA_OPT =	-I../zlib
else
XTRA_OPT =
endif

OBJ	=	rikdataset.o

CPPFLAGS	:=	$(XTRA_OPT)  $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
