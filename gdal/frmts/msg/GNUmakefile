
VPATH = PublicDecompWT/DISE/:PublicDecompWT/COMP/Src/:PublicDecompWT/COMP/WT/Src/

include ../../GDALmake.opt

OBJ1=$(patsubst %.cpp,%.o,$(wildcard PublicDecompWT/COMP/WT/Src/*.cpp))
OBJ2=$(patsubst %.cpp,%.o,$(wildcard PublicDecompWT/COMP/Src/*.cpp))
OBJ3=$(patsubst %.cpp,%.o,$(wildcard PublicDecompWT/DISE/*.cpp))
WTOBJ=$(subst PublicDecompWT/COMP/WT/Src/, ,$(OBJ1)) $(subst PublicDecompWT/COMP/Src/, , $(OBJ2)) $(subst PublicDecompWT/DISE/, , $(OBJ3))

OBJ	=	msgdataset.o xritheaderparser.o prologue.o msgcommand.o reflectancecalculator.o $(WTOBJ)

CPPFLAGS	=	$(GDAL_INCLUDE) -I PublicDecompWT/DISE -I PublicDecompWT/COMP/WT/Inc -I PublicDecompWT/COMP/Inc -I.

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT)) 
