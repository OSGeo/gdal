

include ../../GDALmake.opt

OBJ	=	dted_api.o dteddataset.o dted_create.o dted_ptstream.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o

install-obj:	$(O_OBJ)

dted_test:	dted_test.o $(OBJ)
	$(CXX) -o dted_test dted_test.o dted_api.o dted_create.o dted_ptstream.o \
		 -lgdal -lm 
