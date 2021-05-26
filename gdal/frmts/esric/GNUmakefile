include ../../GDALmake.opt

TARGET = esric

FILES = $(TARGET)_dataset

OBJ = $(FILES:=.$(OBJ_EXT))

PLUGIN_SO = gdal_ESRIC.$(SO_EXT)
PLUGINDIR = $(DESTDIR)$(INST_LIB)/gdalplugins

default: $(OBJ)

clean:
	$(RM) $(O_OBJ) $(OBJ) $(PLUGIN_SO)

install-obj: $(O_OBJ)

plugin: $(PLUGIN_SO)

$(PLUGIN_SO): $(OBJ)
	$(LD) -o $@ -rpath $(PLUGINDIR) -module -avoid-version $^

install-plugin: $(PLUGIN_SO)
	$(INSTALL) $^ $(PLUGINDIR)
