message("Generating ods_formula_parser.cpp")

if (NOT BISON_FOUND)
  message(FATAL_ERROR "Bison not found")
endif()

execute_process(COMMAND "${BISON_EXECUTABLE}" "--no-lines" "-d" "-p" "ods_formula" "-oods_formula_parser.cpp" "ods_formula_parser.y"
                RESULT_VARIABLE STATUS)

if(STATUS AND NOT STATUS EQUAL 0)
  message(FATAL_ERROR "bison failed")
endif()

# Post processing of the generated file
file(READ "ods_formula_parser.cpp" CONTENTS)
# to please clang 15
string(REPLACE "++yynerrs;" "++yynerrs; (void)yynerrs;" CONTENTS "${CONTENTS}")

# to please MSVC that detects that yyerrorlab: label is not reachable
string(REPLACE "yyerrorlab:" "#if 0\nyyerrorlab:" CONTENTS "${CONTENTS}")
string(REPLACE "yyerrlab1:" "#endif\nyyerrlab1:" CONTENTS "${CONTENTS}")

file(WRITE "ods_formula_parser.cpp" "${CONTENTS}")
