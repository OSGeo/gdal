
include ../../../GDALmake.opt

OBJ	= \
	EHapi.o \
	GDapi.o \
	SWapi.o \
	gctp_wrap.o

O_OBJ	=	$(foreach file,$(OBJ),../../o/$(file))

CPPFLAGS	:=	 $(HDF4_INCLUDE) -I. -I.. $(HDF4_FLAGS) $(CPPFLAGS) 

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	$(RM) *.o *.a $(O_OBJ)

install-obj:    $(SUBLIBS) $(O_OBJ:.o=.$(OBJ_EXT))

../../o/%.$(OBJ_EXT):	%.c
	$(CC) $(GDAL_INCLUDE) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
