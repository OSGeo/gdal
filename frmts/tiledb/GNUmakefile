
include ../../GDALmake.opt

OBJ	=	tiledbdataset.o

CPPFLAGS := $(CPPFLAGS) $(TILEDB_CFLAGS)

ifeq ($(findstring c++17,$(TILEDB_CFLAGS)),c++17)
CXX := $(subst -std=c++11,,${CXX})
CXX := $(subst -std=c++14,,${CXX})
endif

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
