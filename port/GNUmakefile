include ../GDALmake.opt


OBJ	=	vsi_simple.o gbserror.o cpl_conv.o

default:	$(OBJ)

clean:
	rm -f *.o

%.o:	%.c
	$(CC) -c $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CC) -c $(CFLAGS) $*.cpp -o $*.o

