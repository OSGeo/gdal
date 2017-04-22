
include ../GDALmake.opt

INST_H_FILES	=	gnm.h gnm_api.h gnmgraph.h

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

install:
	for f in $(INST_H_FILES) ; \
	    do $(INSTALL_DATA) $$f $(DESTDIR)$(INST_INCLUDE) ; \
	done

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
