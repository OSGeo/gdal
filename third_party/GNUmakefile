include ../GDALmake.opt

SUBDIRS     :=

ifeq ($(HAVE_LERC),internal)
SUBDIRS    += LercLib
endif

%-install-obj:
	$(MAKE) -C $* install-obj

default: $(foreach d,$(SUBDIRS),$(d)-install-obj)


clean:  $(foreach d,$(SUBDIRS),$(d)-clean)
	rm -f o/*.o
	$(RM) o/*.lo

