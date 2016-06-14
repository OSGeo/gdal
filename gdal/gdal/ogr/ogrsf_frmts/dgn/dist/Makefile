
DGNLIB_OBJ =	dgnfloat.o dgnhelp.o dgnread.o dgnwrite.o \
			dgnopen.o dgnstroke.o
CPLLIB_OBJ =	cpl_conv.o cpl_dir.o cpl_error.o cpl_multiproc.o \
		cpl_path.o cpl_string.o cpl_vsil_simple.o cpl_vsisimple.o

OBJ = $(CPLLIB_OBJ) $(DGNLIB_OBJ)

default:	dgndump dgnwritetest

dgndump:	dgndump.c $(OBJ)
	$(CXX) dgndump.c $(OBJ) -o dgndump

dgnwritetest:	dgnwritetest.c $(OBJ)
	$(CXX) dgndump.c $(OBJ) -o dgnwritetest

clean:
	rm -f *.o dgndump dgnwritetest
