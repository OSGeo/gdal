
include ../../../GDALmake.opt

CPPFLAGS	:=	-I.. -I../.. $(CPPFLAGS) -std=c++11 

SUBDIRS-yes := libopencad

OBJ	=	ogrcaddriver.o ogrcaddatasource.o ogrcadlayer.o

default:	$(foreach d,$(SUBDIRS-yes),$(d)-target) $(O_OBJ:.o=.$(OBJ_EXT)) 

clean:
	$(foreach d,$(SUBDIRS-yes),$(d)-clean)
	rm -f *.o $(O_OBJ)
	rm -f *~

$(O_OBJ):	ogr_cad.h
