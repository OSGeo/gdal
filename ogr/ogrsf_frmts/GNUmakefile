include ../../GDALmake.opt

default:
	(cd generic; $(MAKE) $*)
	(cd shape; $(MAKE) $*)
	(cd ntf; $(MAKE) $*)
	(cd sdts; $(MAKE) $* default install-libs)
	(cd tiger; $(MAKE) $*)
	(cd s57; $(MAKE) $*)
	(cd mitab; $(MAKE) $*)
	(cd dgn; $(MAKE) $*)
	(cd gml; $(MAKE) $*)
	(cd avc; $(MAKE) $*)
	(cd rec; $(MAKE) $*)
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
	$(MAKE) last-ranlib

last-ranlib:	ogrsf_frmts.a
	$(RANLIB) ogrsf_frmts.a
	touch last-ranlib

clean:
	rm -f ogrsf_frmts.a
	rm -f last-ranlib
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


