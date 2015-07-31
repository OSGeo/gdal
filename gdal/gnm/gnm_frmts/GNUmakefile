
include ../../GDALmake.opt

#config gnm drivers list

#add formats
GNMFILE_ENABLED=yes
GNMDB_ENABLED=yes
#

ifeq ($(GNMFILE_ENABLED),yes)
    CXXFLAGS	:=	$(CXXFLAGS) -DGNMFILE_ENABLED
endif


ifeq ($(GNMDB_ENABLED),yes)
    CXXFLAGS	:=	$(CXXFLAGS) -DGNMDB_ENABLED
endif


SUBDIRS-$(GNMFILE_ENABLED)	+= file
SUBDIRS-$(GNMDB_ENABLED)	+= db

OBJ	=	o/gnmregisterall.o

%-install-obj:
	$(MAKE) -C $* install-obj

default:	$(OBJ:.o=.$(OBJ_EXT))  $(foreach d,$(SUBDIRS-yes),$(d)-install-obj)

o/gnmregisterall.$(OBJ_EXT):	gnmregisterall.cpp ../../GDALmake.opt
	$(CXX) -c $(GDAL_INCLUDE) $(CXXFLAGS) $(FRMT_FLAGS) \
	gnmregisterall.cpp -o o/gnmregisterall.$(OBJ_EXT)

clean:	$(foreach d,$(SUBDIRS-yes),$(d)-clean)
	$(RM) -f *.o o/*.o o/*.a
	$(RM) o/*.lo

