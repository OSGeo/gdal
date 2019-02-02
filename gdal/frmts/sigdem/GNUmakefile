include ../../GDALmake.opt

OBJ	=	sigdemdataset.o

CXXFLAGS        :=      $(WARN_EFFCPLUSPLUS)  $(CXXFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

$(OBJ) $(O_OBJ):	sigdemdataset.h

install:
	$(INSTALL_DATA) sigdemdataset.h $(DESTDIR)$(INST_INCLUDE)
