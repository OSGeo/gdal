message("Generating swq_parser.cpp")

if (NOT BISON_FOUND)
  message(FATAL_ERROR "Bison not found")
endif()

execute_process(COMMAND "${BISON_EXECUTABLE}" "--no-lines" "-d" "-p" "swq" "-oswq_parser.cpp" "swq_parser.y"
                RESULT_VARIABLE STATUS)

if(STATUS AND NOT STATUS EQUAL 0)
  message(FATAL_ERROR "bison failed")
endif()

# Post processing of the generated file
file(READ "swq_parser.cpp" CONTENTS)
# to please clang 15
string(REPLACE "++yynerrs;" "++yynerrs; (void)yynerrs;" CONTENTS "${CONTENTS}")
file(WRITE "swq_parser.cpp" "${CONTENTS}")
