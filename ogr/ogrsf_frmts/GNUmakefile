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
ifeq ($(HAVE_OGDI),yes)
	(cd ogdi; $(MAKE) $*)
endif
ifeq ($(HAVE_OGR_PG),yes)
	(cd pg; $(MAKE) $*)
endif
	$(RANLIB) ogrsf_frmts.a

clean:
	rm -f ogrsf_frmts.a
	(cd generic; $(MAKE) clean)
	(cd shape; $(MAKE) clean)
	(cd ntf; $(MAKE) clean)
	(cd sdts; $(MAKE) clean)
	(cd tiger; $(MAKE) clean)
	(cd s57; $(MAKE) clean)
	(cd mitab; $(MAKE) clean)
	(cd dgn; $(MAKE) clean)
	(cd gml; $(MAKE) clean)
ifeq ($(HAVE_OGDI),yes)
	(cd ogdi; $(MAKE) clean)
endif
ifeq ($(HAVE_OGR_PG),yes)
	(cd pg; $(MAKE) clean)
endif


