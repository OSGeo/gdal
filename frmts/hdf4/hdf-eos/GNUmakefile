
include ../../../GDALmake.opt

OBJ	= \
	EHapi.o \
	GDapi.o \
	SWapi.o \
	gctp_wrap.o

O_OBJ	=	$(foreach file,$(OBJ),../../o/$(file))

CPPFLAGS	:=	$(GDAL_INCLUDE) $(HDF4_INCLUDE) -I. -I.. $(CPPFLAGS) 

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	$(RM) *.o *.a $(O_OBJ)

install-obj:    $(SUBLIBS) $(O_OBJ:.o=.$(OBJ_EXT))

../../o/%.$(OBJ_EXT):	%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
