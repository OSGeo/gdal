

include ../../GDALmake.opt

OBJ	=	dodsdataset2.o 

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) $(DODS_INC)

default:	$(OBJ:.o=.$(OBJ_EXT))

# By linking with dodsdataset.o explicitly, we don't have to build the
# library to get changes in that code into using_dods for testing. However,
# make sure you do rebuild the library (run make two directories up) once
# you're done. 12/27/02 jhrg
using_dods: using_dods.$(OBJ_EXT) dodsdataset.$(OBJ_EXT)
	$(LD) $(LNK_FLAGS) $^ $(XTRAOBJ) $(CONFIG_LIBS) -o $@$(EXE)

# There's a note in GDALmake.opt that local programs should link against
# CONFIG_LIBS, but that doesn't work here, maybe because dodsdataset_test
# links statically (which it needs to do to run in a debugger). 10/31/03 jhrg 
dodsdataset_test: dodsdataset_test.$(OBJ_EXT) dodsdataset.$(OBJ_EXT)
	$(LD) $(LNK_FLAGS) -static -g3 $^ $(XTRAOBJ) $(LIBS) $(LIBGDAL) \
		-lcppunit -lxml2 -o $@$(EXE)

docs:
	doxygen

clean:
	-rm -f *.o *.lo *~ $(O_OBJ)
	-rm -rf docs
	-rm -f dodsdataset_test using_dods

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

dodsdataset.o: dodsdataset.cpp dodsdataset.h
