default:
	(cd generic; $(MAKE) $*)
	(cd shape; $(MAKE) $*)
	(cd ntf; $(MAKE) $*)

clean:
	rm -f ogrsf_frmts.a
	(cd generic; $(MAKE) clean)
	(cd shape; $(MAKE) clean)
	(cd ntf; $(MAKE) clean)
