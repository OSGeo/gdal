include ../GDALmake.opt


OBJ	=	vsi_simple.o

default:	$(OBJ)

clean:
	rm *.o

%.o:	%.c
	$(CC) -c $(CFLAGS) $*.c -o $*.o

%.o:	%.cc
	$(CC) -c $(CFLAGS) $*.cc -o $*.o

