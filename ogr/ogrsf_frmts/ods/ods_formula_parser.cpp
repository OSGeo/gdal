/* A Bison parser, made by GNU Bison 3.5.1.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2020 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Undocumented macros, especially those whose name start with YY_,
   are private implementation details.  Do not rely on them.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.5.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Substitute the variable and function names.  */
#define yyparse ods_formulaparse
#define yylex ods_formulalex
#define yyerror ods_formulaerror
#define yydebug ods_formuladebug
#define yynerrs ods_formulanerrs

/* First part of user prologue.  */

/******************************************************************************
 *
 * Component: OGR ODS Formula Engine
 * Purpose: expression and select parser grammar.
 *          Requires Bison 2.4.0 or newer to process.  Use "make parser" target.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (C) 2010 Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ods_formula.h"

#define YYSTYPE ods_formula_node *

/* Defining YYSTYPE_IS_TRIVIAL is needed because the parser is generated as a
 * C++ file. */
/* See http://www.gnu.org/s/bison/manual/html_node/Memory-Management.html that
 * suggests */
/* increase YYINITDEPTH instead, but this will consume memory. */
/* Setting YYSTYPE_IS_TRIVIAL overcomes this limitation, but might be fragile
 * because */
/* it appears to be a non documented feature of Bison */
#define YYSTYPE_IS_TRIVIAL 1

static void ods_formulaerror(ods_formula_parse_context * /* context */,
                             const char *msg)
{
    CPLError(CE_Failure, CPLE_AppDefined, "Formula Parsing Error: %s", msg);
}

#ifndef YY_CAST
#ifdef __cplusplus
#define YY_CAST(Type, Val) static_cast<Type>(Val)
#define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type>(Val)
#else
#define YY_CAST(Type, Val) ((Type)(Val))
#define YY_REINTERPRET_CAST(Type, Val) ((Type)(Val))
#endif
#endif
#ifndef YY_NULLPTR
#if defined __cplusplus
#if 201103L <= __cplusplus
#define YY_NULLPTR nullptr
#else
#define YY_NULLPTR 0
#endif
#else
#define YY_NULLPTR ((void *)0)
#endif
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
#undef YYERROR_VERBOSE
#define YYERROR_VERBOSE 1
#else
#define YYERROR_VERBOSE 0
#endif

/* Use api.header.include to #include this header
   instead of duplicating it here.  */
#ifndef YY_ODS_FORMULA_ODS_FORMULA_PARSER_HPP_INCLUDED
#define YY_ODS_FORMULA_ODS_FORMULA_PARSER_HPP_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#if YYDEBUG
extern int ods_formuladebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
#define YYTOKENTYPE

enum yytokentype
{
    ODST_NUMBER = 258,
    ODST_STRING = 259,
    ODST_IDENTIFIER = 260,
    ODST_FUNCTION_NO_ARG = 261,
    ODST_FUNCTION_SINGLE_ARG = 262,
    ODST_FUNCTION_TWO_ARG = 263,
    ODST_FUNCTION_THREE_ARG = 264,
    ODST_FUNCTION_ARG_LIST = 265,
    ODST_START = 266,
    ODST_NOT = 267,
    ODST_OR = 268,
    ODST_AND = 269,
    ODST_IF = 270,
    ODST_UMINUS = 271
};
#endif

/* Value type.  */
#if !defined YYSTYPE && !defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
#define YYSTYPE_IS_TRIVIAL 1
#define YYSTYPE_IS_DECLARED 1
#endif

int ods_formulaparse(ods_formula_parse_context *context);

#endif /* !YY_ODS_FORMULA_ODS_FORMULA_PARSER_HPP_INCLUDED  */

#ifdef short
#undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
#include <limits.h> /* INFRINGES ON USER NAME SPACE */
#if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#define YY_STDINT_H
#endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H &&                  \
       UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H &&                 \
       UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
#if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#define YYPTRDIFF_T __PTRDIFF_TYPE__
#define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
#elif defined PTRDIFF_MAX
#ifndef ptrdiff_t
#include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#endif
#define YYPTRDIFF_T ptrdiff_t
#define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
#else
#define YYPTRDIFF_T long
#define YYPTRDIFF_MAXIMUM LONG_MAX
#endif
#endif

#ifndef YYSIZE_T
#ifdef __SIZE_TYPE__
#define YYSIZE_T __SIZE_TYPE__
#elif defined size_t
#define YYSIZE_T size_t
#elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#define YYSIZE_T size_t
#else
#define YYSIZE_T unsigned
#endif
#endif

#define YYSIZE_MAXIMUM                                                         \
    YY_CAST(YYPTRDIFF_T, (YYPTRDIFF_MAXIMUM < YY_CAST(YYSIZE_T, -1)            \
                              ? YYPTRDIFF_MAXIMUM                              \
                              : YY_CAST(YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST(YYPTRDIFF_T, sizeof(X))

/* Stored state numbers (used for stacks). */
typedef yytype_int8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
#if defined YYENABLE_NLS && YYENABLE_NLS
#if ENABLE_NLS
#include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#define YY_(Msgid) dgettext("bison-runtime", Msgid)
#endif
#endif
#ifndef YY_
#define YY_(Msgid) Msgid
#endif
#endif

#ifndef YY_ATTRIBUTE_PURE
#if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#define YY_ATTRIBUTE_PURE __attribute__((__pure__))
#else
#define YY_ATTRIBUTE_PURE
#endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
#if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#define YY_ATTRIBUTE_UNUSED __attribute__((__unused__))
#else
#define YY_ATTRIBUTE_UNUSED
#endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if !defined lint || defined __GNUC__
#define YYUSE(E) ((void)(E))
#else
#define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && !defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                                    \
    _Pragma("GCC diagnostic push")                                             \
        _Pragma("GCC diagnostic ignored \"-Wuninitialized\"")                  \
            _Pragma("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
#define YY_IGNORE_MAYBE_UNINITIALIZED_END _Pragma("GCC diagnostic pop")
#else
#define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
#define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
#define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
#define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && !defined __ICC && 6 <= __GNUC__
#define YY_IGNORE_USELESS_CAST_BEGIN                                           \
    _Pragma("GCC diagnostic push")                                             \
        _Pragma("GCC diagnostic ignored \"-Wuseless-cast\"")
#define YY_IGNORE_USELESS_CAST_END _Pragma("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
#define YY_IGNORE_USELESS_CAST_BEGIN
#define YY_IGNORE_USELESS_CAST_END
#endif

#define YY_ASSERT(E) ((void)(0 && (E)))

#if !defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

#ifdef YYSTACK_USE_ALLOCA
#if YYSTACK_USE_ALLOCA
#ifdef __GNUC__
#define YYSTACK_ALLOC __builtin_alloca
#elif defined __BUILTIN_VA_ARG_INCR
#include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#elif defined _AIX
#define YYSTACK_ALLOC __alloca
#elif defined _MSC_VER
#include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#define alloca _alloca
#else
#define YYSTACK_ALLOC alloca
#if !defined _ALLOCA_H && !defined EXIT_SUCCESS
#include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
/* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#endif
#endif
#endif
#endif

#ifdef YYSTACK_ALLOC
/* Pacify GCC's 'empty if-body' warning.  */
#define YYSTACK_FREE(Ptr)                                                      \
    do                                                                         \
    { /* empty */                                                              \
        ;                                                                      \
    } while (0)
#ifndef YYSTACK_ALLOC_MAXIMUM
/* The OS might guarantee only one guard page at the bottom of the stack,
   and a page size can be as small as 4096 bytes.  So we cannot safely
   invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
   to allow for a few compiler-allocated temporary stack slots.  */
#define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#endif
#else
#define YYSTACK_ALLOC YYMALLOC
#define YYSTACK_FREE YYFREE
#ifndef YYSTACK_ALLOC_MAXIMUM
#define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#endif
#if (defined __cplusplus && !defined EXIT_SUCCESS &&                           \
     !((defined YYMALLOC || defined malloc) &&                                 \
       (defined YYFREE || defined free)))
#include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#endif
#ifndef YYMALLOC
#define YYMALLOC malloc
#if !defined malloc && !defined EXIT_SUCCESS
void *malloc(YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#endif
#endif
#ifndef YYFREE
#define YYFREE free
#if !defined free && !defined EXIT_SUCCESS
void free(void *);      /* INFRINGES ON USER NAME SPACE */
#endif
#endif
#endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */

#if (!defined yyoverflow &&                                                    \
     (!defined __cplusplus ||                                                  \
      (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
    yy_state_t yyss_alloc;
    YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
#define YYSTACK_GAP_MAXIMUM (YYSIZEOF(union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
#define YYSTACK_BYTES(N)                                                       \
    ((N) * (YYSIZEOF(yy_state_t) + YYSIZEOF(YYSTYPE)) + YYSTACK_GAP_MAXIMUM)

#define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
#define YYSTACK_RELOCATE(Stack_alloc, Stack)                                   \
    do                                                                         \
    {                                                                          \
        YYPTRDIFF_T yynewbytes;                                                \
        YYCOPY(&yyptr->Stack_alloc, Stack, yysize);                            \
        Stack = &yyptr->Stack_alloc;                                           \
        yynewbytes = yystacksize * YYSIZEOF(*Stack) + YYSTACK_GAP_MAXIMUM;     \
        yyptr += yynewbytes / YYSIZEOF(*yyptr);                                \
    } while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
#ifndef YYCOPY
#if defined __GNUC__ && 1 < __GNUC__
#define YYCOPY(Dst, Src, Count)                                                \
    __builtin_memcpy(Dst, Src, YY_CAST(YYSIZE_T, (Count)) * sizeof(*(Src)))
#else
#define YYCOPY(Dst, Src, Count)                                                \
    do                                                                         \
    {                                                                          \
        YYPTRDIFF_T yyi;                                                       \
        for (yyi = 0; yyi < (Count); yyi++)                                    \
            (Dst)[yyi] = (Src)[yyi];                                           \
    } while (0)
#endif
#endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL 18
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST 333

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS 34
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS 7
/* YYNRULES -- Number of rules.  */
#define YYNRULES 41
/* YYNSTATES -- Number of states.  */
#define YYNSTATES 108

#define YYUNDEFTOK 2
#define YYMAXUTOK 271

/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                                       \
    (0 <= (YYX) && (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] = {
    0,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2,  2, 2, 2, 2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 30, 2, 2, 2, 21, 18, 2,
    25, 26, 19, 16, 23, 17, 2,  20, 2,  2,  2,  2,  2, 2,  2, 2, 2, 2,  33, 24,
    28, 27, 29, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2,  2, 2, 2, 2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  31, 2, 32, 2, 2, 2, 2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2,  2, 2, 2, 2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2,  2, 2, 2, 2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2,  2, 2, 2, 2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2,  2, 2, 2, 2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2,  2, 2, 2, 2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2,  2, 2, 2, 2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2,  2, 2, 2, 2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2,  2, 2, 1, 2,  3,  4,
    5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 22};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] = {
    0,   89,  89,  94,  94,  98,  103, 108, 113, 119, 126, 134, 141, 148,
    154, 161, 169, 176, 181, 188, 195, 202, 209, 216, 223, 230, 237, 244,
    262, 269, 276, 283, 290, 297, 304, 311, 317, 324, 330, 335, 341, 348};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] = {"$end",
                                      "error",
                                      "$undefined",
                                      "ODST_NUMBER",
                                      "ODST_STRING",
                                      "ODST_IDENTIFIER",
                                      "ODST_FUNCTION_NO_ARG",
                                      "ODST_FUNCTION_SINGLE_ARG",
                                      "ODST_FUNCTION_TWO_ARG",
                                      "ODST_FUNCTION_THREE_ARG",
                                      "ODST_FUNCTION_ARG_LIST",
                                      "ODST_START",
                                      "ODST_NOT",
                                      "ODST_OR",
                                      "ODST_AND",
                                      "ODST_IF",
                                      "'+'",
                                      "'-'",
                                      "'&'",
                                      "'*'",
                                      "'/'",
                                      "'%'",
                                      "ODST_UMINUS",
                                      "','",
                                      "';'",
                                      "'('",
                                      "')'",
                                      "'='",
                                      "'<'",
                                      "'>'",
                                      "'!'",
                                      "'['",
                                      "']'",
                                      "':'",
                                      "$accept",
                                      "input",
                                      "comma",
                                      "value_expr",
                                      "value_expr_list",
                                      "value_expr_and_cell_range_list",
                                      "cell_range",
                                      YY_NULLPTR};
#endif

#ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_int16 yytoknum[] = {
    0,   256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266,
    267, 268, 269, 270, 43,  45,  38,  42,  47,  37,  271, 44,
    59,  40,  41,  61,  60,  62,  33,  91,  93,  58};
#endif

#define YYPACT_NINF (-75)

#define yypact_value_is_default(Yyn) ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) 0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] = {
    -4,  162, 12,  -75, -75, -3,  4,   14,  21,  32,  33,  34,  35,  38,
    162, 162, 44,  303, -75, 40,  162, 162, 162, 182, 162, 162, 162, 162,
    -12, 213, 36,  162, 162, 162, 162, 162, 162, 78,  107, 136, 43,  -75,
    228, 24,  24,  66,  24,  46,  -14, 243, 24,  49,  50,  24,  -75, -75,
    181, 181, 181, -12, -12, -12, 162, 162, 303, 162, 162, 303, 162, 303,
    162, -75, -75, -75, 162, 162, -5,  182, -75, 182, -75, 162, -75, -75,
    162, 303, 303, 303, 303, 303, 303, 258, 24,  72,  -75, -75, -75, 198,
    -75, 162, 47,  -75, 162, 273, -75, 288, -75, -75};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] = {
    0,  0,  0,  5,  6,  0,  0,  0,  0,  0, 0,  0,  0,  0,  0,  0,  0,  2,
    1,  0,  0,  0,  0,  0,  0,  0,  0,  0, 27, 0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  7,  0,  0,  0,  0, 38, 0,  40, 0,  36, 0,  0,  0,
    17, 34, 28, 29, 30, 31, 32, 33, 0,  0, 18, 0,  0,  21, 0,  22, 0,  8,
    3,  4,  0,  0,  0,  0,  16, 0,  13, 0, 12, 11, 0,  24, 25, 23, 19, 26,
    20, 0,  0,  0,  37, 39, 35, 0,  9,  0, 0,  14, 0,  0,  41, 0,  10, 15};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] = {-75, -75, -42, -1, -25, -74, -75};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] = {-1, 2, 74, 46, 51, 47, 48};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int8 yytable[] = {
    17, 52,  75, 94, 77, 95,  79, 1,   81,  72, 73, 84,  18, 28, 29, 37, 38, 39,
    40, 42,  43, 44, 19, 49,  50, 50,  53,  55, 93, 20,  56, 57, 58, 59, 60, 61,
    64, 67,  69, 21, 31, 32,  33, 34,  35,  36, 22, 72,  73, 30, 99, 37, 38, 39,
    40, 102, 96, 23, 24, 25,  26, 85,  86,  27, 87, 88,  41, 89, 55, 90, 70, 76,
    78, 91,  92, 82, 83, 100, 0,  104, 50,  3,  4,  97,  5,  6,  7,  8,  9,  0,
    10, 11,  12, 13, 0,  14,  0,  0,   103, 0,  0,  105, 0,  15, 0,  0,  62, 63,
    0,  16,  3,  4,  0,  5,   6,  7,   8,   9,  0,  10,  11, 12, 13, 0,  14, 0,
    0,  0,   0,  0,  0,  0,   15, 0,   65,  0,  66, 0,   16, 3,  4,  0,  5,  6,
    7,  8,   9,  0,  10, 11,  12, 13,  0,   14, 0,  0,   0,  0,  0,  0,  0,  15,
    0,  68,  0,  3,  4,  16,  5,  6,   7,   8,  9,  0,   10, 11, 12, 13, 0,  14,
    0,  0,   0,  0,  0,  3,   4,  15,  5,   6,  7,  8,   9,  16, 10, 11, 12, 13,
    0,  14,  34, 35, 36, 0,   0,  0,   0,   15, 37, 38,  39, 40, 0,  45, 31, 32,
    33, 34,  35, 36, 0,  72,  73, 0,   101, 37, 38, 39,  40, 31, 32, 33, 34, 35,
    36, 0,   0,  0,  0,  54,  37, 38,  39,  40, 31, 32,  33, 34, 35, 36, 0,  0,
    0,  0,   71, 37, 38, 39,  40, 31,  32,  33, 34, 35,  36, 0,  0,  0,  0,  80,
    37, 38,  39, 40, 31, 32,  33, 34,  35,  36, 0,  0,   0,  0,  98, 37, 38, 39,
    40, 31,  32, 33, 34, 35,  36, 0,   0,   0,  0,  106, 37, 38, 39, 40, 31, 32,
    33, 34,  35, 36, 0,  0,   0,  0,   107, 37, 38, 39,  40, 31, 32, 33, 34, 35,
    36, 0,   0,  0,  0,  0,   37, 38,  39,  40};

static const yytype_int8 yycheck[] = {
    1,  26, 44, 77, 46, 79, 48,  11, 50, 23, 24, 53, 0,  14, 15, 27, 28, 29, 30,
    20, 21, 22, 25, 24, 25, 26,  27, 32, 33, 25, 31, 32, 33, 34, 35, 36, 37, 38,
    39, 25, 16, 17, 18, 19, 20,  21, 25, 23, 24, 5,  92, 27, 28, 29, 30, 97, 81,
    25, 25, 25, 25, 62, 63, 25,  65, 66, 26, 68, 32, 70, 27, 5,  26, 74, 75, 26,
    26, 5,  -1, 32, 81, 3,  4,   84, 6,  7,  8,  9,  10, -1, 12, 13, 14, 15, -1,
    17, -1, -1, 99, -1, -1, 102, -1, 25, -1, -1, 28, 29, -1, 31, 3,  4,  -1, 6,
    7,  8,  9,  10, -1, 12, 13,  14, 15, -1, 17, -1, -1, -1, -1, -1, -1, -1, 25,
    -1, 27, -1, 29, -1, 31, 3,   4,  -1, 6,  7,  8,  9,  10, -1, 12, 13, 14, 15,
    -1, 17, -1, -1, -1, -1, -1,  -1, -1, 25, -1, 27, -1, 3,  4,  31, 6,  7,  8,
    9,  10, -1, 12, 13, 14, 15,  -1, 17, -1, -1, -1, -1, -1, 3,  4,  25, 6,  7,
    8,  9,  10, 31, 12, 13, 14,  15, -1, 17, 19, 20, 21, -1, -1, -1, -1, 25, 27,
    28, 29, 30, -1, 31, 16, 17,  18, 19, 20, 21, -1, 23, 24, -1, 26, 27, 28, 29,
    30, 16, 17, 18, 19, 20, 21,  -1, -1, -1, -1, 26, 27, 28, 29, 30, 16, 17, 18,
    19, 20, 21, -1, -1, -1, -1,  26, 27, 28, 29, 30, 16, 17, 18, 19, 20, 21, -1,
    -1, -1, -1, 26, 27, 28, 29,  30, 16, 17, 18, 19, 20, 21, -1, -1, -1, -1, 26,
    27, 28, 29, 30, 16, 17, 18,  19, 20, 21, -1, -1, -1, -1, 26, 27, 28, 29, 30,
    16, 17, 18, 19, 20, 21, -1,  -1, -1, -1, 26, 27, 28, 29, 30, 16, 17, 18, 19,
    20, 21, -1, -1, -1, -1, -1,  27, 28, 29, 30};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_int8 yystos[] = {
    0,  11, 35, 3,  4,  6,  7,  8,  9,  10, 12, 13, 14, 15, 17, 25, 31, 37,
    0,  25, 25, 25, 25, 25, 25, 25, 25, 25, 37, 37, 5,  16, 17, 18, 19, 20,
    21, 27, 28, 29, 30, 26, 37, 37, 37, 31, 37, 39, 40, 37, 37, 38, 38, 37,
    26, 32, 37, 37, 37, 37, 37, 37, 28, 29, 37, 27, 29, 37, 27, 37, 27, 26,
    23, 24, 36, 36, 5,  36, 26, 36, 26, 36, 26, 26, 36, 37, 37, 37, 37, 37,
    37, 37, 37, 33, 39, 39, 38, 37, 26, 36, 5,  26, 36, 37, 32, 37, 26, 26};

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_int8 yyr1[] = {0,  34, 35, 36, 36, 37, 37, 37, 37, 37, 37,
                                   37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
                                   37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
                                   37, 37, 38, 38, 39, 39, 39, 39, 40};

/* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_int8 yyr2[] = {0, 2, 2, 1, 1, 1, 1, 3, 4, 6, 8, 4, 4, 4,
                                   6, 8, 4, 3, 3, 4, 4, 3, 3, 4, 4, 4, 4, 2,
                                   3, 3, 3, 3, 3, 3, 3, 3, 1, 3, 1, 3, 1, 5};

#define yyerrok (yyerrstatus = 0)
#define yyclearin (yychar = YYEMPTY)
#define YYEMPTY (-2)
#define YYEOF 0

#define YYACCEPT goto yyacceptlab
#define YYABORT goto yyabortlab
#define YYERROR goto yyerrorlab

#define YYRECOVERING() (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                                 \
    do                                                                         \
        if (yychar == YYEMPTY)                                                 \
        {                                                                      \
            yychar = (Token);                                                  \
            yylval = (Value);                                                  \
            YYPOPSTACK(yylen);                                                 \
            yystate = *yyssp;                                                  \
            goto yybackup;                                                     \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            yyerror(context, YY_("syntax error: cannot back up"));             \
            YYERROR;                                                           \
        }                                                                      \
    while (0)

/* Error token number */
#define YYTERROR 1
#define YYERRCODE 256

/* Enable debugging if requested.  */
#if YYDEBUG

#ifndef YYFPRINTF
#include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#define YYFPRINTF fprintf
#endif

#define YYDPRINTF(Args)                                                        \
    do                                                                         \
    {                                                                          \
        if (yydebug)                                                           \
            YYFPRINTF Args;                                                    \
    } while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
#define YY_LOCATION_PRINT(File, Loc) ((void)0)
#endif

#define YY_SYMBOL_PRINT(Title, Type, Value, Location)                          \
    do                                                                         \
    {                                                                          \
        if (yydebug)                                                           \
        {                                                                      \
            YYFPRINTF(stderr, "%s ", Title);                                   \
            yy_symbol_print(stderr, Type, Value, context);                     \
            YYFPRINTF(stderr, "\n");                                           \
        }                                                                      \
    } while (0)

/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void yy_symbol_value_print(FILE *yyo, int yytype,
                                  YYSTYPE const *const yyvaluep,
                                  ods_formula_parse_context *context)
{
    FILE *yyoutput = yyo;
    YYUSE(yyoutput);
    YYUSE(context);
    if (!yyvaluep)
        return;
#ifdef YYPRINT
    if (yytype < YYNTOKENS)
        YYPRINT(yyo, yytoknum[yytype], *yyvaluep);
#endif
    YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
    YYUSE(yytype);
    YY_IGNORE_MAYBE_UNINITIALIZED_END
}

/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void yy_symbol_print(FILE *yyo, int yytype,
                            YYSTYPE const *const yyvaluep,
                            ods_formula_parse_context *context)
{
    YYFPRINTF(yyo, "%s %s (", yytype < YYNTOKENS ? "token" : "nterm",
              yytname[yytype]);

    yy_symbol_value_print(yyo, yytype, yyvaluep, context);
    YYFPRINTF(yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void yy_stack_print(yy_state_t *yybottom, yy_state_t *yytop)
{
    YYFPRINTF(stderr, "Stack now");
    for (; yybottom <= yytop; yybottom++)
    {
        int yybot = *yybottom;
        YYFPRINTF(stderr, " %d", yybot);
    }
    YYFPRINTF(stderr, "\n");
}

#define YY_STACK_PRINT(Bottom, Top)                                            \
    do                                                                         \
    {                                                                          \
        if (yydebug)                                                           \
            yy_stack_print((Bottom), (Top));                                   \
    } while (0)

/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void yy_reduce_print(yy_state_t *yyssp, YYSTYPE *yyvsp, int yyrule,
                            ods_formula_parse_context *context)
{
    int yylno = yyrline[yyrule];
    int yynrhs = yyr2[yyrule];
    int yyi;
    YYFPRINTF(stderr, "Reducing stack by rule %d (line %d):\n", yyrule - 1,
              yylno);
    /* The symbols being reduced.  */
    for (yyi = 0; yyi < yynrhs; yyi++)
    {
        YYFPRINTF(stderr, "   $%d = ", yyi + 1);
        yy_symbol_print(stderr, yystos[+yyssp[yyi + 1 - yynrhs]],
                        &yyvsp[(yyi + 1) - (yynrhs)], context);
        YYFPRINTF(stderr, "\n");
    }
}

#define YY_REDUCE_PRINT(Rule)                                                  \
    do                                                                         \
    {                                                                          \
        if (yydebug)                                                           \
            yy_reduce_print(yyssp, yyvsp, Rule, context);                      \
    } while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
#define YYDPRINTF(Args)
#define YY_SYMBOL_PRINT(Title, Type, Value, Location)
#define YY_STACK_PRINT(Bottom, Top)
#define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */

/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
#define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif

#if YYERROR_VERBOSE

#ifndef yystrlen
#if defined __GLIBC__ && defined _STRING_H
#define yystrlen(S) (YY_CAST(YYPTRDIFF_T, strlen(S)))
#else
/* Return the length of YYSTR.  */
static YYPTRDIFF_T yystrlen(const char *yystr)
{
    YYPTRDIFF_T yylen;
    for (yylen = 0; yystr[yylen]; yylen++)
        continue;
    return yylen;
}
#endif
#endif

#ifndef yystpcpy
#if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#define yystpcpy stpcpy
#else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *yystpcpy(char *yydest, const char *yysrc)
{
    char *yyd = yydest;
    const char *yys = yysrc;

    while ((*yyd++ = *yys++) != '\0')
        continue;

    return yyd - 1;
}
#endif
#endif

#ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYPTRDIFF_T yytnamerr(char *yyres, const char *yystr)
{
    if (*yystr == '"')
    {
        YYPTRDIFF_T yyn = 0;
        char const *yyp = yystr;

        for (;;)
            switch (*++yyp)
            {
                case '\'':
                case ',':
                    goto do_not_strip_quotes;

                case '\\':
                    if (*++yyp != '\\')
                        goto do_not_strip_quotes;
                    else
                        goto append;

                append:
                default:
                    if (yyres)
                        yyres[yyn] = *yyp;
                    yyn++;
                    break;

                case '"':
                    if (yyres)
                        yyres[yyn] = '\0';
                    return yyn;
            }
    do_not_strip_quotes:;
    }

    if (yyres)
        return yystpcpy(yyres, yystr) - yyres;
    else
        return yystrlen(yystr);
}
#endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int yysyntax_error(YYPTRDIFF_T *yymsg_alloc, char **yymsg,
                          yy_state_t *yyssp, int yytoken)
{
    enum
    {
        YYERROR_VERBOSE_ARGS_MAXIMUM = 5
    };

    /* Internationalized format string. */
    const char *yyformat = YY_NULLPTR;
    /* Arguments of yyformat: reported tokens (one for the "unexpected",
       one per "expected"). */
    char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
    /* Actual size of YYARG. */
    int yycount = 0;
    /* Cumulated lengths of YYARG.  */
    YYPTRDIFF_T yysize = 0;

    /* There are many possibilities here to consider:
       - If this state is a consistent state with a default action, then
         the only way this function was invoked is if the default action
         is an error action.  In that case, don't check for expected
         tokens because there are none.
       - The only way there can be no lookahead present (in yychar) is if
         this state is a consistent state with a default action.  Thus,
         detecting the absence of a lookahead is sufficient to determine
         that there is no unexpected or expected token to report.  In that
         case, just report a simple "syntax error".
       - Don't assume there isn't a lookahead just because this state is a
         consistent state with a default action.  There might have been a
         previous inconsistent state, consistent state with a non-default
         action, or user semantic action that manipulated yychar.
       - Of course, the expected token list depends on states to have
         correct lookahead information, and it depends on the parser not
         to perform extra reductions after fetching a lookahead from the
         scanner and before detecting a syntax error.  Thus, state merging
         (from LALR or IELR) and default reductions corrupt the expected
         token list.  However, the list is correct for canonical LR with
         one exception: it will still contain any token that will not be
         accepted due to an error action in a later state.
    */
    if (yytoken != YYEMPTY)
    {
        int yyn = yypact[+*yyssp];
        YYPTRDIFF_T yysize0 = yytnamerr(YY_NULLPTR, yytname[yytoken]);
        yysize = yysize0;
        yyarg[yycount++] = yytname[yytoken];
        if (!yypact_value_is_default(yyn))
        {
            /* Start YYX at -YYN if negative to avoid negative indexes in
               YYCHECK.  In other words, skip the first -YYN actions for
               this state because they are default actions.  */
            int yyxbegin = yyn < 0 ? -yyn : 0;
            /* Stay within bounds of both yycheck and yytname.  */
            int yychecklim = YYLAST - yyn + 1;
            int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
            int yyx;

            for (yyx = yyxbegin; yyx < yyxend; ++yyx)
                if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR &&
                    !yytable_value_is_error(yytable[yyx + yyn]))
                {
                    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                    {
                        yycount = 1;
                        yysize = yysize0;
                        break;
                    }
                    yyarg[yycount++] = yytname[yyx];
                    {
                        YYPTRDIFF_T yysize1 =
                            yysize + yytnamerr(YY_NULLPTR, yytname[yyx]);
                        if (yysize <= yysize1 &&
                            yysize1 <= YYSTACK_ALLOC_MAXIMUM)
                            yysize = yysize1;
                        else
                            return 2;
                    }
                }
        }
    }

    switch (yycount)
    {
#define YYCASE_(N, S)                                                          \
    case N:                                                                    \
        yyformat = S;                                                          \
        break
        default: /* Avoid compiler warnings. */
            YYCASE_(0, YY_("syntax error"));
            YYCASE_(1, YY_("syntax error, unexpected %s"));
            YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
            YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
            YYCASE_(
                4,
                YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
            YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or "
                           "%s or %s"));
#undef YYCASE_
    }

    {
        /* Don't count the "%s"s in the final size, but reserve room for
           the terminator.  */
        YYPTRDIFF_T yysize1 = yysize + (yystrlen(yyformat) - 2 * yycount) + 1;
        if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
            yysize = yysize1;
        else
            return 2;
    }

    if (*yymsg_alloc < yysize)
    {
        *yymsg_alloc = 2 * yysize;
        if (!(yysize <= *yymsg_alloc && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
            *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
        return 1;
    }

    /* Avoid sprintf, as that infringes on the user's name space.
       Don't have undefined behavior even if the translation
       produced a string with the wrong number of "%s"s.  */
    {
        char *yyp = *yymsg;
        int yyi = 0;
        while ((*yyp = *yyformat) != '\0')
            if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
            {
                yyp += yytnamerr(yyp, yyarg[yyi++]);
                yyformat += 2;
            }
            else
            {
                ++yyp;
                ++yyformat;
            }
    }
    return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void yydestruct(const char *yymsg, int yytype, YYSTYPE *yyvaluep,
                       ods_formula_parse_context *context)
{
    YYUSE(yyvaluep);
    YYUSE(context);
    if (!yymsg)
        yymsg = "Deleting";
    YY_SYMBOL_PRINT(yymsg, yytype, yyvaluep, yylocationp);

    YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
    switch (yytype)
    {
        case 3: /* ODST_NUMBER  */
        {
            delete (*yyvaluep);
        }
        break;

        case 4: /* ODST_STRING  */
        {
            delete (*yyvaluep);
        }
        break;

        case 5: /* ODST_IDENTIFIER  */
        {
            delete (*yyvaluep);
        }
        break;

        case 6: /* ODST_FUNCTION_NO_ARG  */
        {
            delete (*yyvaluep);
        }
        break;

        case 7: /* ODST_FUNCTION_SINGLE_ARG  */
        {
            delete (*yyvaluep);
        }
        break;

        case 8: /* ODST_FUNCTION_TWO_ARG  */
        {
            delete (*yyvaluep);
        }
        break;

        case 9: /* ODST_FUNCTION_THREE_ARG  */
        {
            delete (*yyvaluep);
        }
        break;

        case 10: /* ODST_FUNCTION_ARG_LIST  */
        {
            delete (*yyvaluep);
        }
        break;

        case 37: /* value_expr  */
        {
            delete (*yyvaluep);
        }
        break;

        case 38: /* value_expr_list  */
        {
            delete (*yyvaluep);
        }
        break;

        case 39: /* value_expr_and_cell_range_list  */
        {
            delete (*yyvaluep);
        }
        break;

        case 40: /* cell_range  */
        {
            delete (*yyvaluep);
        }
        break;

        default:
            break;
    }
    YY_IGNORE_MAYBE_UNINITIALIZED_END
}

/*----------.
| yyparse.  |
`----------*/

int yyparse(ods_formula_parse_context *context)
{
    /* The lookahead symbol.  */
    int yychar;

    /* The semantic value of the lookahead symbol.  */
    /* Default value used for initialization, for pacifying older GCCs
       or non-GCC compilers.  */
    YY_INITIAL_VALUE(static YYSTYPE yyval_default;)
    YYSTYPE yylval YY_INITIAL_VALUE(= yyval_default);

    /* Number of syntax errors so far.  */
    int yynerrs;

    yy_state_fast_t yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss;
    yy_state_t *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYPTRDIFF_T yystacksize;

    int yyn;
    int yyresult;
    /* Lookahead token as an internal (translated) token number.  */
    int yytoken = 0;
    /* The variables used to return semantic value and location from the
       action routines.  */
    YYSTYPE yyval;

#if YYERROR_VERBOSE
    /* Buffer for error messages, and its allocated size.  */
    char yymsgbuf[128];
    char *yymsg = yymsgbuf;
    YYPTRDIFF_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N) (yyvsp -= (N), yyssp -= (N))

    /* The number of symbols on the RHS of the reduced rule.
       Keep to zero when no symbol should be popped.  */
    int yylen = 0;

    yyssp = yyss = yyssa;
    yyvsp = yyvs = yyvsa;
    yystacksize = YYINITDEPTH;

    YYDPRINTF((stderr, "Starting parse\n"));

    yystate = 0;
    yyerrstatus = 0;
    yynerrs = 0;
    yychar = YYEMPTY; /* Cause a token to be read.  */
    goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
    /* In all cases, when you get here, the value and location stacks
       have just been pushed.  So pushing a state here evens the stacks.  */
    yyssp++;

/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
    YYDPRINTF((stderr, "Entering state %d\n", yystate));
    YY_ASSERT(0 <= yystate && yystate < YYNSTATES);
    YY_IGNORE_USELESS_CAST_BEGIN
    *yyssp = YY_CAST(yy_state_t, yystate);
    YY_IGNORE_USELESS_CAST_END

    if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
        goto yyexhaustedlab;
#else
    {
        /* Get the current used size of the three stacks, in elements.  */
        YYPTRDIFF_T yysize = yyssp - yyss + 1;

#if defined yyoverflow
        {
            /* Give user a chance to reallocate the stack.  Use copies of
               these so that the &'s don't force the real ones into
               memory.  */
            yy_state_t *yyss1 = yyss;
            YYSTYPE *yyvs1 = yyvs;

            /* Each stack pointer address is followed by the size of the
               data in use in that stack, in bytes.  This used to be a
               conditional around just the two extra args, but that might
               be undefined if yyoverflow is a macro.  */
            yyoverflow(YY_("memory exhausted"), &yyss1,
                       yysize * YYSIZEOF(*yyssp), &yyvs1,
                       yysize * YYSIZEOF(*yyvsp), &yystacksize);
            yyss = yyss1;
            yyvs = yyvs1;
        }
#else /* defined YYSTACK_RELOCATE */
        /* Extend the stack our own way.  */
        if (YYMAXDEPTH <= yystacksize)
            goto yyexhaustedlab;
        yystacksize *= 2;
        if (YYMAXDEPTH < yystacksize)
            yystacksize = YYMAXDEPTH;

        {
            yy_state_t *yyss1 = yyss;
            union yyalloc *yyptr = YY_CAST(
                union yyalloc *,
                YYSTACK_ALLOC(YY_CAST(YYSIZE_T, YYSTACK_BYTES(yystacksize))));
            if (!yyptr)
                goto yyexhaustedlab;
            YYSTACK_RELOCATE(yyss_alloc, yyss);
            YYSTACK_RELOCATE(yyvs_alloc, yyvs);
#undef YYSTACK_RELOCATE
            if (yyss1 != yyssa)
                YYSTACK_FREE(yyss1);
        }
#endif

        yyssp = yyss + yysize - 1;
        yyvsp = yyvs + yysize - 1;

        YY_IGNORE_USELESS_CAST_BEGIN
        YYDPRINTF((stderr, "Stack size increased to %ld\n",
                   YY_CAST(long, yystacksize)));
        YY_IGNORE_USELESS_CAST_END

        if (yyss + yystacksize - 1 <= yyssp)
            YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */

    if (yystate == YYFINAL)
        YYACCEPT;

    goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:
    /* Do appropriate processing given the current state.  Read a
       lookahead token if we need one and don't already have one.  */

    /* First try to decide what to do without reference to lookahead token.  */
    yyn = yypact[yystate];
    if (yypact_value_is_default(yyn))
        goto yydefault;

    /* Not known => get a lookahead token if don't already have one.  */

    /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
    if (yychar == YYEMPTY)
    {
        YYDPRINTF((stderr, "Reading a token: "));
        yychar = yylex(&yylval, context);
    }

    if (yychar <= YYEOF)
    {
        yychar = yytoken = YYEOF;
        YYDPRINTF((stderr, "Now at end of input.\n"));
    }
    else
    {
        yytoken = YYTRANSLATE(yychar);
        YY_SYMBOL_PRINT("Next token is", yytoken, &yylval, &yylloc);
    }

    /* If the proper action on seeing token YYTOKEN is to reduce or to
       detect an error, take that action.  */
    yyn += yytoken;
    if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
        goto yydefault;
    yyn = yytable[yyn];
    if (yyn <= 0)
    {
        if (yytable_value_is_error(yyn))
            goto yyerrlab;
        yyn = -yyn;
        goto yyreduce;
    }

    /* Count tokens shifted since error; after three, turn off error
       status.  */
    if (yyerrstatus)
        yyerrstatus--;

    /* Shift the lookahead token.  */
    YY_SYMBOL_PRINT("Shifting", yytoken, &yylval, &yylloc);
    yystate = yyn;
    YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
    *++yyvsp = yylval;
    YY_IGNORE_MAYBE_UNINITIALIZED_END

    /* Discard the shifted token.  */
    yychar = YYEMPTY;
    goto yynewstate;

/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
    yyn = yydefact[yystate];
    if (yyn == 0)
        goto yyerrlab;
    goto yyreduce;

/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
    /* yyn is the number of a rule to reduce with.  */
    yylen = yyr2[yyn];

    /* If YYLEN is nonzero, implement the default value of the action:
       '$$ = $1'.

       Otherwise, the following line sets YYVAL to garbage.
       This behavior is undocumented and Bison
       users should not rely upon it.  Assigning to YYVAL
       unconditionally makes the parser a bit smaller, and it avoids a
       GCC warning that YYVAL may be used uninitialized.  */
    yyval = yyvsp[1 - yylen];

    YY_REDUCE_PRINT(yyn);
    switch (yyn)
    {
        case 2:
        {
            context->poRoot = yyvsp[0];
        }
        break;

        case 5:
        {
            yyval = yyvsp[0];
        }
        break;

        case 6:
        {
            yyval = yyvsp[0];
        }
        break;

        case 7:
        {
            yyval = yyvsp[-2];
        }
        break;

        case 8:
        {
            yyval = yyvsp[-3];
            yyval->PushSubExpression(yyvsp[-1]);
        }
        break;

        case 9:
        {
            yyval = yyvsp[-5];
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[-1]);
        }
        break;

        case 10:
        {
            yyval = yyvsp[-7];
            yyval->PushSubExpression(yyvsp[-5]);
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[-1]);
        }
        break;

        case 11:
        {
            yyval = new ods_formula_node(ODS_AND);
            yyvsp[-1]->ReverseSubExpressions();
            yyval->PushSubExpression(yyvsp[-1]);
        }
        break;

        case 12:
        {
            yyval = new ods_formula_node(ODS_OR);
            yyvsp[-1]->ReverseSubExpressions();
            yyval->PushSubExpression(yyvsp[-1]);
        }
        break;

        case 13:
        {
            yyval = new ods_formula_node(ODS_NOT);
            yyval->PushSubExpression(yyvsp[-1]);
        }
        break;

        case 14:
        {
            yyval = new ods_formula_node(ODS_IF);
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[-1]);
        }
        break;

        case 15:
        {
            yyval = new ods_formula_node(ODS_IF);
            yyval->PushSubExpression(yyvsp[-5]);
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[-1]);
        }
        break;

        case 16:
        {
            yyval = yyvsp[-3];
            yyvsp[-1]->ReverseSubExpressions();
            yyval->PushSubExpression(yyvsp[-1]);
        }
        break;

        case 17:
        {
            yyval = yyvsp[-1];
        }
        break;

        case 18:
        {
            yyval = new ods_formula_node(ODS_EQ);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 19:
        {
            yyval = new ods_formula_node(ODS_NE);
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 20:
        {
            yyval = new ods_formula_node(ODS_NE);
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 21:
        {
            yyval = new ods_formula_node(ODS_LT);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 22:
        {
            yyval = new ods_formula_node(ODS_GT);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 23:
        {
            yyval = new ods_formula_node(ODS_LE);
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 24:
        {
            yyval = new ods_formula_node(ODS_LE);
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 25:
        {
            yyval = new ods_formula_node(ODS_LE);
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 26:
        {
            yyval = new ods_formula_node(ODS_GE);
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 27:
        {
            if (yyvsp[0]->eNodeType == SNT_CONSTANT &&
                !(yyvsp[0]->field_type == ODS_FIELD_TYPE_INTEGER &&
                  yyvsp[0]->int_value == INT_MIN))
            {
                yyval = yyvsp[0];
                yyval->int_value *= -1;
                yyval->float_value *= -1;
            }
            else
            {
                yyval = new ods_formula_node(ODS_MULTIPLY);
                yyval->PushSubExpression(new ods_formula_node(-1));
                yyval->PushSubExpression(yyvsp[0]);
            }
        }
        break;

        case 28:
        {
            yyval = new ods_formula_node(ODS_ADD);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 29:
        {
            yyval = new ods_formula_node(ODS_SUBTRACT);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 30:
        {
            yyval = new ods_formula_node(ODS_CONCAT);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 31:
        {
            yyval = new ods_formula_node(ODS_MULTIPLY);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 32:
        {
            yyval = new ods_formula_node(ODS_DIVIDE);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 33:
        {
            yyval = new ods_formula_node(ODS_MODULUS);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 34:
        {
            yyval = new ods_formula_node(ODS_CELL);
            yyval->PushSubExpression(yyvsp[-1]);
        }
        break;

        case 35:
        {
            yyval = yyvsp[0];
            yyvsp[0]->PushSubExpression(yyvsp[-2]);
        }
        break;

        case 36:
        {
            yyval = new ods_formula_node(ODS_LIST);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 37:
        {
            yyval = yyvsp[0];
            yyvsp[0]->PushSubExpression(yyvsp[-2]);
        }
        break;

        case 38:
        {
            yyval = new ods_formula_node(ODS_LIST);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 39:
        {
            yyval = yyvsp[0];
            yyvsp[0]->PushSubExpression(yyvsp[-2]);
        }
        break;

        case 40:
        {
            yyval = new ods_formula_node(ODS_LIST);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 41:
        {
            yyval = new ods_formula_node(ODS_CELL_RANGE);
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[-1]);
        }
        break;

        default:
            break;
    }
    /* User semantic actions sometimes alter yychar, and that requires
       that yytoken be updated with the new translation.  We take the
       approach of translating immediately before every use of yytoken.
       One alternative is translating here after every semantic action,
       but that translation would be missed if the semantic action invokes
       YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
       if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
       incorrect destructor might then be invoked immediately.  In the
       case of YYERROR or YYBACKUP, subsequent parser actions might lead
       to an incorrect destructor call or verbose syntax error message
       before the lookahead is translated.  */
    YY_SYMBOL_PRINT("-> $$ =", yyr1[yyn], &yyval, &yyloc);

    YYPOPSTACK(yylen);
    yylen = 0;
    YY_STACK_PRINT(yyss, yyssp);

    *++yyvsp = yyval;

    /* Now 'shift' the result of the reduction.  Determine what state
       that goes to, based on the state we popped back to and the rule
       number reduced by.  */
    {
        const int yylhs = yyr1[yyn] - YYNTOKENS;
        const int yyi = yypgoto[yylhs] + *yyssp;
        yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
                       ? yytable[yyi]
                       : yydefgoto[yylhs]);
    }

    goto yynewstate;

/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
    /* Make sure we have latest lookahead translation.  See comments at
       user semantic actions for why this is necessary.  */
    yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE(yychar);

    /* If not already recovering from an error, report this error.  */
    if (!yyerrstatus)
    {
        ++yynerrs;
        (void)yynerrs;
#if !YYERROR_VERBOSE
        yyerror(context, YY_("syntax error"));
#else
#define YYSYNTAX_ERROR yysyntax_error(&yymsg_alloc, &yymsg, yyssp, yytoken)
        {
            char const *yymsgp = YY_("syntax error");
            int yysyntax_error_status;
            yysyntax_error_status = YYSYNTAX_ERROR;
            if (yysyntax_error_status == 0)
                yymsgp = yymsg;
            else if (yysyntax_error_status == 1)
            {
                if (yymsg != yymsgbuf)
                    YYSTACK_FREE(yymsg);
                yymsg = YY_CAST(char *,
                                YYSTACK_ALLOC(YY_CAST(YYSIZE_T, yymsg_alloc)));
                if (!yymsg)
                {
                    yymsg = yymsgbuf;
                    yymsg_alloc = sizeof yymsgbuf;
                    yysyntax_error_status = 2;
                }
                else
                {
                    yysyntax_error_status = YYSYNTAX_ERROR;
                    yymsgp = yymsg;
                }
            }
            yyerror(context, yymsgp);
            if (yysyntax_error_status == 2)
                goto yyexhaustedlab;
        }
#undef YYSYNTAX_ERROR
#endif
    }

    if (yyerrstatus == 3)
    {
        /* If just tried and failed to reuse lookahead token after an
           error, discard it.  */

        if (yychar <= YYEOF)
        {
            /* Return failure if at end of input.  */
            if (yychar == YYEOF)
                YYABORT;
        }
        else
        {
            yydestruct("Error: discarding", yytoken, &yylval, context);
            yychar = YYEMPTY;
        }
    }

    /* Else will try to reuse lookahead token after shifting the error
       token.  */
    goto yyerrlab1;

/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
#if 0
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
#endif
yyerrlab1:
    yyerrstatus = 3; /* Each real token shifted decrements this.  */

    for (;;)
    {
        yyn = yypact[yystate];
        if (!yypact_value_is_default(yyn))
        {
            yyn += YYTERROR;
            if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
            {
                yyn = yytable[yyn];
                if (0 < yyn)
                    break;
            }
        }

        /* Pop the current state because it cannot handle the error token.  */
        if (yyssp == yyss)
            YYABORT;

        yydestruct("Error: popping", yystos[yystate], yyvsp, context);
        YYPOPSTACK(1);
        yystate = *yyssp;
        YY_STACK_PRINT(yyss, yyssp);
    }

    YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
    *++yyvsp = yylval;
    YY_IGNORE_MAYBE_UNINITIALIZED_END

    /* Shift the error token.  */
    YY_SYMBOL_PRINT("Shifting", yystos[yyn], yyvsp, yylsp);

    yystate = yyn;
    goto yynewstate;

/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
    yyresult = 0;
    goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
    yyresult = 1;
    goto yyreturn;

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
    yyerror(context, YY_("memory exhausted"));
    yyresult = 2;
    /* Fall through.  */
#endif

/*-----------------------------------------------------.
| yyreturn -- parsing is finished, return the result.  |
`-----------------------------------------------------*/
yyreturn:
    if (yychar != YYEMPTY)
    {
        /* Make sure we have latest lookahead translation.  See comments at
           user semantic actions for why this is necessary.  */
        yytoken = YYTRANSLATE(yychar);
        yydestruct("Cleanup: discarding lookahead", yytoken, &yylval, context);
    }
    /* Do not reclaim the symbols of the rule whose action triggered
       this YYABORT or YYACCEPT.  */
    YYPOPSTACK(yylen);
    YY_STACK_PRINT(yyss, yyssp);
    while (yyssp != yyss)
    {
        yydestruct("Cleanup: popping", yystos[+*yyssp], yyvsp, context);
        YYPOPSTACK(1);
    }
#ifndef yyoverflow
    if (yyss != yyssa)
        YYSTACK_FREE(yyss);
#endif
#if YYERROR_VERBOSE
    if (yymsg != yymsgbuf)
        YYSTACK_FREE(yymsg);
#endif
    return yyresult;
}
