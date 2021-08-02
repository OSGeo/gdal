include ../../GDALmake.opt

OBJ	=	stacitdataset.o

CXXFLAGS        :=      $(WARN_EFFCPLUSPLUS) $(CXXFLAGS) -iquote ../vrt

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

install:
