
GDAL_ROOT	=	..

include ../GDALmake.opt

OBJ	=	o/gdalallregister.o

FRMT_FLAGS	=	$(foreach FRMT, $(GDAL_FORMATS), -DFRMT_$(FRMT))

ALL_C_FLAGS	=	$(GDAL_INCLUDE) $(CFLAGS) $(FRMT_FLAGS)

default:	$(OBJ) sub-libs

sub-libs:
	@for dir in $(GDAL_FORMATS) ; do \
	  if test -d $$dir ; then \
	    (cd $$dir; $(MAKE) install-obj ); \
	  else \
	    echo "Can't find:" $$dir; \
	  fi; \
	done

clean:
	rm -f *.o o/*.o o/*.a
	@for dir in $(GDAL_FORMATS) ; do \
	  if test -d $$dir ; then \
	    (cd $$dir; $(MAKE) clean ); \
	  else \
	    echo "Can't find:" $$dir; \
	  fi; \
	done

o/gdalallregister.o:	gdalallregister.cpp
	$(CXX) -c $(ALL_C_FLAGS) -DGDAL_FORMATS="$(GDAL_FORMATS)" \
		-I../core gdalallregister.cpp -o o/gdalallregister.o






