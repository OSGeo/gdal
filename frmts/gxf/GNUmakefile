WEB_DIR =	/u/www/projects/gxf

GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	gxfopen.o gxfdataset.o gxf_proj4.o

ALL_C_FLAGS	=	$(GDAL_INCLUDE) $(CFLAGS)



default:	$(OBJ)

clean:
	rm -f *.o

%.o:	%.cpp
	$(CXX) -c $(ALL_C_FLAGS) -I$(GDAL_ROOT)/core -I$(GDAL_ROOT)/port \
		$*.cpp -o $*.o

%.o:	%.c
	$(CC) -c $(ALL_C_FLAGS) -I$(GDAL_ROOT)/core -I$(GDAL_ROOT)/port \
		$*.c -o $*.o


gxftest:	gxftest.o gxfopen.o
	$(CXX) gxftest.o gxfopen.o $(GDAL_ROOT)/port/*.o $(LIBS) -o gxftest

install-obj:	$(foreach file,$(OBJ),../o/$(file))

../o/%.o:	%.o
	cp $*.o ../o

docs:
	rm -rf html
	mkdir html
	doxygen

update-web:	docs
	cp html/* $(WEB_DIR)


