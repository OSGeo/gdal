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


include ../gdal/GDALmake.opt

ifndef PYTHON
        PYTHON=python
endif

PYTHON_LIB := $(shell $(PYTHON) -c "import sysconfig; print(sysconfig.get_config_var('BLDLIBRARY'))" )
PYTHON_INCLUDE_DIRS := $(shell $(PYTHON) -c "import sysconfig; print(sysconfig.get_path('include'))" )
CXXFLAGS += -I$(PYTHON_INCLUDE_DIRS)

pytest_runner: pytest_runner.o
	$(CXX) $(LDFLAGS) $< $(PYTHON_LIB) -o $@

pytest_runner.o: pytest_runner.cpp
	$(CXX) $(CXXFLAGS) -c $<
