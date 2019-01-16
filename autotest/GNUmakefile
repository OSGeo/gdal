all:
	@echo "Run 'make -j test'"

gcore-test:
	pytest gcore

gdrivers-test:
	pytest gdrivers

alg-test:
	pytest alg

osr-test:
	pytest osr

ogr-test:
	pytest ogr

utilities-test:
	pytest utilities

pyscripts-test:
	pytest pyscripts

test check:
	@$(MAKE) $(MFLAGS) gcore-test gdrivers-test alg-test ogr-test osr-test utilities-test pyscripts-test
