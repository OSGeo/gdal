main_interface = ../gma.i

interface = $(main_interface) typemaps.i links.i

module = gma.pm

bindings = $(module) gma_wrap.cxx gma_wrap.o gma.bs Makefile pm_to_blib MYMETA.json MYMETA.yml -r blib

all: $(module) Makefile gma_wrap.o

$(module): $(interface) gma_wrap.cxx

gma_wrap.cxx: $(interface)
	swig -c++ -perl5 -outcurrentdir $(main_interface)

Makefile: Makefile.PL
	perl Makefile.PL

gma_wrap.o: Makefile
	make -f Makefile

install: gma_wrap.o
	make -f Makefile install

test: gma_wrap.o
	make -f Makefile test

clean:
	rm -f $(bindings)
