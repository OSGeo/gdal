
include ../GDALmake.opt

OBJ	=	gnmnetwork.o gnmgenericnetwork.o gnmlayer.o gnmrule.o gnmresultlayer.o \
        gnmgraph.o

default:	lib

all:	lib

clean:
	rm -f html/*
	(cd gnm_frmts; $(MAKE) clean)
	$(RM) *.o

very-clean: clean
	rm -rf html rtf

lib:	sublibs $(OBJ:.o=.$(OBJ_EXT))

$(OBJ): gnm.h gnm_priv.h 

obj:	$(OBJ)

sublibs:
	(cd gnm_frmts; $(MAKE))
	
install:	;

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
	
