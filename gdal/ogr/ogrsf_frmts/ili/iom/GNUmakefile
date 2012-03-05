

include ../../../../GDALmake.opt

OBJ =	iom_attr.o iom_basket.o iom_error.o iom_file.o iom_iterator.o iom_object.o iom_utilities.o iom_value.o \
    reader.o tags.o ustrings.o writer.o

O_OBJ = $(foreach file,$(OBJ),../../o/$(file))

CPPFLAGS	:=	$(XERCES_INCLUDE) $(CPPFLAGS) -I..

default:	 $(O_OBJ:.o=.$(OBJ_EXT))

../../o/%.$(OBJ_EXT):       %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	iom.h iom_p.h
