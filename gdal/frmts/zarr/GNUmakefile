
OBJ	=	zarrdriver.o

include ../../GDALmake.opt

XTRA_OPT=
ifeq ($(HAVE_BLOSC),yes)
XTRA_OPT := $(XTRA_OPT) -DHAVE_BLOSC
endif

CPPFLAGS :=  -I../mem -I../gtiff $(CPPFLAGS) $(XTRA_OPT)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
