include ../GDALmake.opt

SUBDIRS-yes     :=

SUBDIRS-$(HAVE_LERC)    += LercLib

%-install-obj:
	$(MAKE) -C $* install-obj

default: $(foreach d,$(SUBDIRS-yes),$(d)-install-obj)


clean:  $(foreach d,$(SUBDIRS-yes),$(d)-clean)
	rm -f o/*.o
	$(RM) o/*.lo

