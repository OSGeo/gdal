main_interface = ../gma.i

interface = $(main_interface) typemaps.i links.i

module = gma.py

bindings = $(module) gma_wrap.cxx gma_wrap.o _gma.so gma.pyc -r build

all: $(module) gma_wrap.o

$(module): $(interface) gma_wrap.cxx

gma_wrap.cxx: $(interface)
	swig -c++ -python -outcurrentdir $(main_interface)

gma_wrap.o:
	python setup.py build_ext --inplace

install: gma_wrap.o
	make -f Makefile install

test: gma_wrap.o
	make -f Makefile test

clean:
	rm -f $(bindings)
