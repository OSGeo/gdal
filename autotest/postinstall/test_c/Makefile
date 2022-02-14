PROGRAM = test_c
OBJECTS = test_c.o

ifdef GDAL_CONFIG
override CFLAGS += -g -Wall -Werror $(shell $(GDAL_CONFIG) --cflags)
override LDFLAGS += $(shell $(GDAL_CONFIG) --libs)
else
override CFLAGS += -g -Wall -Werror $(shell pkg-config gdal --cflags)
override LDFLAGS += $(shell pkg-config gdal --libs)
endif

all: $(PROGRAM)

$(PROGRAM): $(OBJECTS)
	$(CC) -o $@ $< $(LDFLAGS)

clean:
	$(RM) $(PROGRAM) $(OBJECTS)

.PHONY: clean
