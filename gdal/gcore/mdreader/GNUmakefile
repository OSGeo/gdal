
include ../../GDALmake.opt

OBJ	=	reader_digital_globe.o reader_geo_eye.o reader_orb_view.o \
                reader_pleiades.o reader_rdk1.o reader_landsat.o

UP_OBJ   = $(addprefix ../,$(OBJ))

default:	$(UP_OBJ:.o=.$(OBJ_EXT))

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

../%.$(OBJ_EXT):	%.c
	$(CC) $(GDAL_INCLUDE) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

../%.$(OBJ_EXT):	%.cpp
	$(CXX) $(GDAL_INCLUDE) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

%.$(OBJ_EXT):	%.c
	$(CC) $(GDAL_INCLUDE) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

%.$(OBJ_EXT):	%.cpp
	$(CXX) $(GDAL_INCLUDE) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

clean:
	$(RM) *.o $(O_OBJ)
