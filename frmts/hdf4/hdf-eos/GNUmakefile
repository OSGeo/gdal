GDAL_ROOT	=	../../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	= \
	EHapi.o \
	GDapi.o \
	PTapi.o \
	SWapi.o \
	gctp_wrap.o

O_OBJ	=	$(foreach file,$(OBJ),../../o/$(file))

ALL_C_FLAGS	=	  $(CFLAGS) $(HDF4_INCLUDE) -I.

default:	$(OBJ)

clean:
	rm -f $(O_OBJ) *.o *.a

install-obj:	$(O_OBJ)

../../o/%.o:	%.c
	$(CC) -c -I$(GDAL_ROOT)/port $(ALL_C_FLAGS) $< -o $@
