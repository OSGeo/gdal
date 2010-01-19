default:	
	(cd src; $(MAKE))
	(cd tests; $(MAKE))

clean:
	(cd src; $(MAKE) clean)
	(cd tests; $(MAKE) clean)
	
check:	default
	(cd tests; $(MAKE) check)
