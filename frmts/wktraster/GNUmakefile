# USER CONFIGURATION
# END OF USER CONFIGURATION 


include ../../GDALmake.opt

OBJ	=	wktrasterdataset.o wktrasterrasterband.o wktrasterwrapper.o


CPPFLAGS	:= $(XTRA_OPT) $(PG_INC) $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

$(O_OBJ):       wktraster.h
 
clean:
	rm -f *.o $(O_OBJ)

../o/%.$(OBJ_EXT):
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

all:	$(OBJ:.o=.$(OBJ_EXT))

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
