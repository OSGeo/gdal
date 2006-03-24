# USER CONFIGURATION
# Set your postgis include path here :
POSTGIS_INC     =-I/path/to/your/postgis/headers
# END OF USER CONFIGURATION 


include ../../GDALmake.opt

OBJ	=	pgchipdataset.o pgchiprasterband.o pgchiputilities.o


CPPFLAGS	:=	$(XTRA_OPT) $(PG_INC) $(POSTGIS_INC) $(GDAL_INCLUDE) $(CPPFLAGS )

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)

../o/%.o:
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

all:	$(OBJ)

install-obj:	$(O_OBJ)
