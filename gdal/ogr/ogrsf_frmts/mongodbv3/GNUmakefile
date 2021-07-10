

include ../../../GDALmake.opt

OBJ	=	ogrmongodbv3driver.o

# -foperator-names needed for mongocxx headers...
CXXFLAGS := $(subst -fno-operator-names,,${CXXFLAGS})

CPPFLAGS	:=	-iquote ../generic $(MONGOCXXV3_CFLAGS) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
