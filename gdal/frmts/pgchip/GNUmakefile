# USER CONFIGURATION
# Set your postgis include path here :
POSTGIS_INC     =-I/path/to/your/postgis/headers
# END OF USER CONFIGURATION 


include ../../GDALmake.opt

OBJ	=	pgchipdataset.o pgchiprasterband.o pgchiputilities.o


CPPFLAGS	:= -Wall -g $(XTRA_OPT) $(PG_INC) $(POSTGIS_INC)  $(CPPFLAGS )

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

../o/%.$(OBJ_EXT):
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

all:	$(OBJ:.o=.$(OBJ_EXT))

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
