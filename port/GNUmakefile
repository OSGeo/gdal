include ../GDALmake.opt


OBJ	=	cpl_conv.o cpl_error.o cpl_string.o cpl_vsisimple.o

default:	$(OBJ)

clean:
	rm -f *.o

%.o:	%.c
	$(CC) -c $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CC) -c $(CFLAGS) $*.cpp -o $*.o

