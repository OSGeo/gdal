

include ../../../GDALmake.opt

OBJ	=	ogrdodsdriver.o ogrdodsdatasource.o ogrdodslayer.o \
		ogrdodssequencelayer.o ogrdodsfielddefn.o ogrdodsgrid.o

CPPFLAGS	:=	-I..  $(CPPFLAGS) $(DODS_INC) $(LIBXML2_INC) -DDO_NOT_USE_DEBUG_BOOL

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)


dodstest:	dodstest.cpp
	g++ -g -I$(DODS_INC) dodstest.cpp -o dodstest -L/u/pkg/DODS/lib -ldap++ -lpthread -lrx  -L/u/pkg/dods/DODS/lib -lcurl -lssl -lcrypto -L/u/pkg/dods/DODS/lib -lxml2 -L/u/pkg/dods/DODS/lib -ldl -lz

