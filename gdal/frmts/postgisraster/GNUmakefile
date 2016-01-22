# USER CONFIGURATION
# END OF USER CONFIGURATION 


include ../../GDALmake.opt

OBJ	=	postgisrasterdriver.o postgisrasterdataset.o postgisrasterrasterband.o postgisrastertiledataset.o postgisrastertilerasterband.o postgisrastertools.o


CPPFLAGS	:= -I ../mem -I ../vrt $(XTRA_OPT) $(PG_INC) $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

$(O_OBJ):       postgisraster.h ../vrt/vrtdataset.h ../mem/memdataset.h
 
clean:
	rm -f *.o $(O_OBJ)

../o/%.$(OBJ_EXT):
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

all:	$(OBJ:.o=.$(OBJ_EXT))

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
