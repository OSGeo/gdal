
include ../../GDALmake.opt

OBJ	=	jpeg2000dataset.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(JASPER_FLAGS) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f $(OBJ) $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
