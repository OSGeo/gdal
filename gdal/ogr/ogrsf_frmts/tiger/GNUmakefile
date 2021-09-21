include ../../../GDALmake.opt

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
	   tigerspatialmetadata.o     \
	   tigertlidrange.o           \
	   tigerzerocellid.o          \
	   tigeroverunder.o           \
	   tigerzipplus4.o	      \
	   tigerpoint.o

CPPFLAGS	:=	-iquote .. -iquote ../..  $(CPPFLAGS)

LIBS	:=	$(GDAL_LIB) $(LIBS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

$(O_OBJ): ogr_tiger.h

clean:
	rm -f *.o $(O_OBJ)
