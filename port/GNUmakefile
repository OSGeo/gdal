#
# CPL (Common Portability Library) makefile
#

ifneq ($(wildcard ../GDALmake.op?),)
include ../GDALmake.opt
else
include GDALmake.opt
endif

OBJ	=	cpl_conv.o cpl_error.o cpl_string.o cpl_vsisimple.o \
		cplgetsymbol.o cpl_dir.o

LIB	=	cpl.a

default:	$(OBJ)

$(LIB):		$(OBJ)

clean:
	rm -f *.o

%.o:	%.c
	$(CC) -c $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(CFLAGS) $*.cpp -o $*.o

$(LIB): $(OBJ)
	rm -f $(LIB)
	$(AR) rc $(LIB) $(OBJ)
