#
# CPL (Common Portability Library) makefile
#

ifneq ($(wildcard ../GDALmake.op?),)
include ../GDALmake.opt
else
include GDALmake.opt
endif

OBJ	=	cpl_conv.o cpl_error.o cpl_string.o cpl_vsisimple.o \
		cplgetsymbol.o cpl_dir.o cpl_path.o

LIB	=	cpl.a

default:	$(LIB)

clean:
	rm -f *.o $(LIB)

%.o:	%.c
	$(CC) -c $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(CXXFLAGS) $*.cpp -o $*.o

$(LIB): $(OBJ)
	rm -f $(LIB)
	$(AR) rc $(LIB) $(OBJ)
