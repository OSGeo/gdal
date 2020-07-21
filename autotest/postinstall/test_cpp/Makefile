PROGRAM = test_cpp
OBJECTS = test_cpp.o

override CXXFLAGS += -std=c++11 -g -Wall -Werror $(shell pkg-config gdal --cflags)
override LDFLAGS += $(shell pkg-config gdal --libs)

all: $(PROGRAM)

$(PROGRAM): $(OBJECTS)
	$(CXX) -o $@ $< $(LDFLAGS)

clean:
	$(RM) $(PROGRAM) $(OBJECTS)

.PHONY: clean
