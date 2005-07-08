
GDAL_ROOT	=	../../..

SUBDIRS-yes     := iom

include $(GDAL_ROOT)/GDALmake.opt

CORE_OBJ =	 ili1reader.o ili2reader.o

OGR_OBJ =	iomhelper.o ogrili1driver.o ogrili1datasource.o ogrili1layer.o ogrili2driver.o ogrili2datasource.o ogrili2layer.o

OBJ =	$(CORE_OBJ) $(OGR_OBJ)

CPPFLAGS	:=	$(XERCES_INCLUDE) -I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS)
#CFLAGS	:=	$(filter-out -Wall,$(CFLAGS))

ifeq ($(HAVE_XERCES),yes)
CORE_OBJ :=	$(CORE_OBJ) ili2handler.o
CPPFLAGS +=  -DHAVE_XERCES=1
else
CPPFLAGS +=  -DHAVE_XERCES=0
endif

# By default, XML validation is disabled.  Uncomment the following line to
# enable XML schema validation in the parser.
#CPPFLAGS +=  -DOGR_ILI2_VALIDATION=1

default:	$(foreach d,$(SUBDIRS-yes),$(d)-target)	$(O_OBJ)

clean: $(foreach d,$(SUBDIRS-yes),$(d)-clean)
	rm -f *.o $(O_OBJ)
