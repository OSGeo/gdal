
include ../../GDALmake.opt

CPPFLAGS := $(XTRA_OPT) $(GDAL_INCLUDE) $(CPPFLAGS)

ifeq ($(PCRASTER_SETTING),internal)
CPPFLAGS += -DUSE_IN_GDAL -Ilibcsf
OBJ = _getcell.o _getrow.o _gsomece.o _putcell.o _rputrow.o angle.o attravai.o attrsize.o cellsize.o create2.o csfglob.o csfsup.o delattr.o dumconv.o endian.o filename.o gattrblk.o gattridx.o gcellrep.o gdattype.o getattr.o getx0.o gety0.o ggisfid.o gmaxval.o gminval.o gnrcols.o gnrrows.o gproj.o gputproj.o gvalscal.o gvartype.o gversion.o ismv.o kernlcsf.o legend.o mclose.o mopen.o moreattr.o mperror.o pgisfid.o pmaxval.o pminval.o putallmv.o putattr.o putsomec.o putx0.o puty0.o pvalscal.o rattrblk.o rcomp.o rcoords.o rdup2.o reseterr.o rextend.o rmalloc.o rrowcol.o ruseas.o setangle.o setmv.o setvtmv.o strconst.o strpad.o swapio.o trackmm.o vs2.o vsdef.o vsis.o vsvers.o wattrblk.o
endif

OBJ += pcrasterdataset.o pcrastermisc.o pcrasterrasterband.o pcrasterutil.o

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o libcsf/*.o $(O_OBJ)
	rm -fR html

../o/%.$(OBJ_EXT): libcsf/%.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

docs:
	doxygen doxygen.cfg

ctags:
	ctags *cpp *h
