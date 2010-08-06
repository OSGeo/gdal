
include ../GDALmake.opt

OBJ	=	gdalopeninfo.o gdaldrivermanager.o gdaldriver.o gdaldataset.o \
		gdalrasterband.o gdal_misc.o rasterio.o gdalrasterblock.o \
		gdalcolortable.o gdalmajorobject.o overview.o \
		gdaldefaultoverviews.o gdalpamdataset.o gdalpamrasterband.o \
		gdaljp2metadata.o gdaljp2box.o gdalmultidomainmetadata.o \
		gdal_rat.o gdalgmlcoverage.o gdalpamproxydb.o \
		gdalallvalidmaskband.o gdalnodatamaskband.o gdal_rpcimdio.o \
 		gdalproxydataset.o gdalproxypool.o gdaldefaultasync.o \
		gdalnodatavaluesmaskband.o

# Enable the following if you want to use MITAB's code to convert
# .tab coordinate systems into well known text.  But beware that linking
# against static libraries becomes more complicated because of the odd
# call out.

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) $(PAM_SETTING)

default:	$(OBJ:.o=.$(OBJ_EXT))

$(OBJ):	gdal_priv.h

clean:
	$(RM) *.o $(O_OBJ)

docs:	
	(cd ..; $(MAKE) docs)

gdal_misc.$(OBJ_EXT):	gdal_misc.cpp gdal_version.h

gdaldrivermanager.$(OBJ_EXT):	gdaldrivermanager.cpp ../GDALmake.opt
	$(CXX) -c $(GDAL_INCLUDE) $(CXXFLAGS) -DINST_DATA=\"$(INST_DATA)\" \
		$< -o $@

install:
	for f in *.h ; do $(INSTALL_DATA) $$f $(DESTDIR)$(INST_INCLUDE) ; done
