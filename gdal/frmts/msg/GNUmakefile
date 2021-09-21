include ../../GDALmake.opt

OBJ	=	msgdataset.o xritheaderparser.o prologue.o msgcommand.o reflectancecalculator.o PublicDecompWT_all.o

CPPFLAGS	:=	 -DDO_NOT_USE_DEBUG_BOOL -I PublicDecompWT/DISE -I PublicDecompWT/COMP/WT/Inc -I PublicDecompWT/COMP/Inc -iquote . $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o PublicDecompWT_all.h

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

PublicDecompWT_all.cpp: PublicDecompWT_all.h

# Trick to avoid lots of warnings from the PublicDecompWT SDK
PublicDecompWT_all.h:
	@echo "// This is a generated file. Do not edit !" > PublicDecompWT_all.h
	@echo "#if ((__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2)) && !defined(_MSC_VER))" >> PublicDecompWT_all.h
	@echo "#pragma GCC system_header" >> PublicDecompWT_all.h
	@echo "#endif" >> PublicDecompWT_all.h
	for x in $(wildcard PublicDecompWT/COMP/WT/Src/*.cpp) $(wildcard PublicDecompWT/COMP/Src/*.cpp) $(wildcard PublicDecompWT/DISE/*.cpp) ; do\
		echo "#include \"$$x\"" >> PublicDecompWT_all.h; \
	done
