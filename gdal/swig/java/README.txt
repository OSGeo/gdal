
        The shared library with the JNI interface can include the main
        GDAL library or not.

        For Windows, simply ensure that DLLBUILD is not defined by
        commenting out its definition in $(GDAL_ROOT)/nmake.opt.

        For Linux, run 'make' with INCLUDE_GDAL_LIB defined, e.g.,

            make INCLUDE_GDAL_LIB=1
