default:
	(cd generic; $(MAKE) $*)
	(cd shape; $(MAKE) $*)
	(cd ntf; $(MAKE) $*)
	(cd sdts; $(MAKE) $* default install-libs)
	(cd tiger; $(MAKE) $*)
	(cd s57; $(MAKE) $*)

clean:
	rm -f ogrsf_frmts.a
	(cd generic; $(MAKE) clean)
	(cd shape; $(MAKE) clean)
	(cd ntf; $(MAKE) clean)
	(cd sdts; $(MAKE) clean)
	(cd s57; $(MAKE) clean)


