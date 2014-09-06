all:
	@echo "Run 'make -j test'"

gcore-test:
	python run_all.py gcore

gdrivers-test:
	python run_all.py gdrivers

alg-test:
	python run_all.py alg

osr-test:
	python run_all.py osr

ogr-test:
	python run_all.py ogr

utilities-test:
	python run_all.py utilities

pyscripts-test:
	python run_all.py pyscripts

test:
	@$(MAKE) $(MFLAGS) gcore-test gdrivers-test alg-test ogr-test osr-test utilities-test pyscripts-test
