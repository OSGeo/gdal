
GDAL_ROOT	=	../../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ     =  ogrtigerdriver.o           \
	   ogrtigerdatasource.o       \
	   ogrtigerlayer.o            \
	   tigerfilebase.o            \
	   tigercompletechain.o       \
	   tigeraltname.o             \
	   tigerfeatureids.o          \
	   tigerzipcodes.o            \
	   tigerlandmarks.o           \
	   tigerarealandmarks.o       \
	   tigerkeyfeatures.o         \
	   tigerpolygon.o             \
	   tigerpolygoncorrections.o  \
	   tigerentitynames.o         \
	   tigerpolygoneconomic.o     \
	   tigeridhistory.o           \
	   tigerpolychainlink.o       \
	   tigerpip.o                 \
	   tigertlidrange.o           \
	   tigerzerocellid.o          \
	   tigeroverunder.o           \
	   tigerzipplus4.o	      \
	   tigerpoint.o



ALL_C_FLAGS	=	-I.. -I../.. -I$(GDAL_ROOT)/port $(CFLAGS)

LIBS	:=	../ogrsf_frmts.a ../../ogr.a $(CPL_LIB) $(LIBS)


default:	$(O_OBJ)

clean:
	rm -f *.o

%.o:	%.cpp
	$(CXX) -c $(ALL_C_FLAGS) $*.cpp -o $*.o

%.o:	%.c
	$(CC) -c $(ALL_C_FLAGS) $*.c -o $*.o

tigerinfo:	tigerinfo.o $(OBJ)
	$(CXX) $(LNK_FLAGS) tigerinfo.o $(LIBS) -o tigerinfo
