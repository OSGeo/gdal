PROGRAM = test_cpp
OBJECTS = test_cpp.o

ifdef GDAL_CONFIG
override CXXFLAGS += -std=c++11 -g -Wall -Werror $(shell $(GDAL_CONFIG) --cflags)
override LDFLAGS += $(shell $(GDAL_CONFIG) --libs)
else
override CXXFLAGS += -std=c++11 -g -Wall -Werror $(shell pkg-config gdal --cflags)
override LDFLAGS += $(shell pkg-config gdal --libs)
endif

all: $(PROGRAM)

$(PROGRAM): $(OBJECTS)
	$(CXX) -o $@ $< $(LDFLAGS)

clean:
	$(RM) $(PROGRAM) $(OBJECTS)

.PHONY: clean
