This directory contain fuzzer main funtions and scripts for the
Google OSS Fuzz project: https://github.com/google/oss-fuzz/

The main build scripts are in:
https://github.com/google/oss-fuzz/tree/master/projects/gdal
and call scripts in this directory.

The list of issues is in:
https://bugs.chromium.org/p/oss-fuzz/issues/list?q=gdal

- Simple build of fuzzer main functions:
    $ make

- Simulate the build of (dummy) fuzzers like OSS Fuzz does:
    $ make dummyfuzzers
  They are created in /tmp/*_fuzzer as well as with the
  /tmp/*_fuzzer_seed_corpus.zip files
  
  Run one:
    $ /tmp/ogr_fuzzer a_file_name

- Run locally OSS Fuzz:
    $ git clone https://github.com/google/oss-fuzz.git
    $ cd oss-fuzz
    $ export PROJECT_NAME=gdal
    $ python infra/helper.py build_image $PROJECT_NAME

  Build fuzzers with the address sanitizer (could use undefined, etc...)
    $ python infra/helper.py build_fuzzers --sanitizer address $PROJECT_NAME

  Test a particular fuzzer (replace shape_fuzzer by other fuzzers
  like the ones generated in /tmp by "make dummyfuzzers")
    $ python infra/helper.py run_fuzzer $PROJECT_NAME shape_fuzzer
