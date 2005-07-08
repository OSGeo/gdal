
GDAL_ROOT	=	../../../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ =	iom_attr.o iom_basket.o iom_error.o iom_file.o iom_iterator.o iom_object.o iom_utilities.o iom_value.o \
    reader.o tags.o ustrings.o writer.o

O_OBJ = $(foreach file,$(OBJ),../../o/$(file))

CPPFLAGS	:=	$(XERCES_INCLUDE) $(CPPFLAGS) -I..
#CFLAGS	:=	$(filter-out -Wall,$(CFLAGS))

ifeq ($(HAVE_XERCES),yes)
CPPFLAGS +=  -DHAVE_XERCES=1
else
CPPFLAGS +=  -DHAVE_XERCES=0
endif

default:	 $(O_OBJ)

../../o/%.o:       %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

clean:
	rm -f *.o $(O_OBJ)
