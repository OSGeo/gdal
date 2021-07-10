

include ../../../GDALmake.opt

OBJ	=	ogrlvbagdriver.o ogrlvbagdatasource.o ogrlvbaglayer.o

ifeq ($(HAVE_EXPAT),yes)
CPPFLAGS += -DHAVE_EXPAT
endif

ifeq ($(HAVE_GEOS),yes)
CPPFLAGS += -DHAVE_GEOS $(GEOS_CFLAGS)
endif

CPPFLAGS	:=	-iquote .. -iquote ../.. -iquote ../generic $(EXPAT_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_lvbag.h
