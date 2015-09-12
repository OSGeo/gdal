

include ../../../GDALmake.opt

OBJ	=	ogrwaspdriver.o ogrwaspdatasource.o ogrwasplayer.o

CPPFLAGS	:=	-I.. -I../..  $(CPPFLAGS)

# Enable this to compile with -Wextra -Werror with auto_ptr warning.
# CPPFLAGS += -Wno-deprecated-declarations

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ:.o=.$(OBJ_EXT)):	ogrwasp.h
