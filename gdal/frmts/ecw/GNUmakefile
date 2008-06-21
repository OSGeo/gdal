
include ../../GDALmake.opt

OBJ	=	ecwdataset.o ecwcreatecopy.o jp2userbox.o

CPPFLAGS	:=	$(GDAL_INCLUDE) -DFRMT_ecw $(CPPFLAGS) $(EXTRA_CFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ)

plugin: gdal_ECW_JP2ECW.so

gdal_ECW.so:	$(OBJ)
	$(LD_SHARED) $(LNK_FLAGS) $(OBJ) $(CONFIG_LIBS) $(EXTRA_LIBS) \
		-o gdal_ECW_JP2ECW.so
