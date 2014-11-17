

VERSION =	1.3

include ../../GDALmake.opt

OBJ	=	bsb_read.o bsbdataset.o

DISTDIR =	bsb-$(VERSION)



default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

bsb2raw$(EXE):	bsb2raw.$(OBJ_EXT)
	$(LD) $(LDFLAGS) bsb2raw.$(OBJ_EXT) $(CONFIG_LIBS) -o bsb2raw$(EXE)


install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

dist:	
	rm -rf $(DISTDIR)
	mkdir $(DISTDIR)
	cp *.cpp *.c *.h $(DISTDIR)
	rm $(DISTDIR)/bsbdataset.cpp
	cp Makefile.dist $(DISTDIR)/Makefile
	cp README.dist $(DISTDIR)/README
	cp ../../port/{cpl_error*,cpl_port*,cpl_string*} $(DISTDIR)
	cp ../../port/{cpl_vsisimple.cpp,cpl_config.h.in} $(DISTDIR)
	cp ../../port/{cpl_vsi.h,cpl_conv.*,cpl_path.cpp} $(DISTDIR)
	cp ../../port/cpl_config.h.in $(DISTDIR)/cpl_config.h
	rm $(DISTDIR)/*.o
	tar czf $(DISTDIR).tar.gz $(DISTDIR)
	zip -r $(DISTDIR).zip $(DISTDIR)
