include ../../GDALmake.opt

default:	check-lib

check-lib:	sub-libs
	$(MAKE) ogrsf_frmts.a

ogrsf_frmts.a:	./o/*.o
	ar r $@ $^
	$(RANLIB) ogrsf_frmts.a

sub-libs:
	(cd generic; $(MAKE) $*)
	(cd shape; $(MAKE) $*)
	(cd ntf; $(MAKE) $*)
	(cd sdts; $(MAKE) $*)
	(cd tiger; $(MAKE) $*)
	(cd s57; $(MAKE) $*)
	(cd mitab; $(MAKE) $*)
	(cd dgn; $(MAKE) $*)
	(cd gml; $(MAKE) $*)
	(cd avc; $(MAKE) $*)
	(cd rec; $(MAKE) $*)
	(cd mem; $(MAKE) $*)
ifeq ($(HAVE_OGDI),yes)
	(cd ogdi; $(MAKE) $*)
endif
ifeq ($(HAVE_OCI),yes)
	(cd oci; $(MAKE) $*)
endif
ifeq ($(HAVE_OGR_PG),yes)
	(cd pg; $(MAKE) $*)
endif
ifeq ($(HAVE_FME),yes)
	(cd fme; $(MAKE) $*)
endif

clean:
	rm -f ogrsf_frmts.a
	rm o/*.o
	(cd generic; $(MAKE) clean)
	(cd shape; $(MAKE) clean)
	(cd ntf; $(MAKE) clean)
	(cd sdts; $(MAKE) clean)
	(cd tiger; $(MAKE) clean)
	(cd s57; $(MAKE) clean)
	(cd mitab; $(MAKE) clean)
	(cd dgn; $(MAKE) clean)
	(cd gml; $(MAKE) clean)
	(cd avc; $(MAKE) clean)
	(cd rec; $(MAKE) clean)
	(cd mem; $(MAKE) clean)
ifeq ($(HAVE_OGDI),yes)
	(cd ogdi; $(MAKE) clean)
endif
ifeq ($(HAVE_OCI),yes)
	(cd oci; $(MAKE) clean)
endif
ifeq ($(HAVE_OGR_PG),yes)
	(cd pg; $(MAKE) clean)
endif
ifeq ($(HAVE_FME),yes)
	(cd fme; $(MAKE) clean)
endif


