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
#define yyparse swqparse
#define yylex swqlex
#define yyerror swqerror
#define yydebug swqdebug
#define yynerrs swqnerrs

/* First part of user prologue.  */

/******************************************************************************
 *
 * Component: OGR SQL Engine
 * Purpose: expression and select parser grammar.
 *          Requires Bison 2.4.0 or newer to process.  Use "make parser" target.
 * Author: Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (C) 2010 Frank Warmerdam <warmerdam@pobox.com>
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

#include "cpl_port.h"
#include "ogr_swq.h"

#include <cstdlib>
#include <cstring>
#include <limits>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "ogr_core.h"
#include "ogr_geometry.h"

#define YYSTYPE swq_expr_node *

/* Defining YYSTYPE_IS_TRIVIAL is needed because the parser is generated as a
 * C++ file. */
/* See http://www.gnu.org/s/bison/manual/html_node/Memory-Management.html that
 * suggests */
/* increase YYINITDEPTH instead, but this will consume memory. */
/* Setting YYSTYPE_IS_TRIVIAL overcomes this limitation, but might be fragile
 * because */
/* it appears to be a non documented feature of Bison */
#define YYSTYPE_IS_TRIVIAL 1

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
#define YYERROR_VERBOSE 1
#endif

/* Use api.header.include to #include this header
   instead of duplicating it here.  */
#ifndef YY_SWQ_SWQ_PARSER_HPP_INCLUDED
#define YY_SWQ_SWQ_PARSER_HPP_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#if YYDEBUG
extern int swqdebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
#define YYTOKENTYPE
enum yytokentype
{
    END = 0,
    SWQT_INTEGER_NUMBER = 258,
    SWQT_FLOAT_NUMBER = 259,
    SWQT_STRING = 260,
    SWQT_IDENTIFIER = 261,
    SWQT_IN = 262,
    SWQT_LIKE = 263,
    SWQT_ILIKE = 264,
    SWQT_ESCAPE = 265,
    SWQT_BETWEEN = 266,
    SWQT_NULL = 267,
    SWQT_IS = 268,
    SWQT_SELECT = 269,
    SWQT_LEFT = 270,
    SWQT_JOIN = 271,
    SWQT_WHERE = 272,
    SWQT_ON = 273,
    SWQT_ORDER = 274,
    SWQT_BY = 275,
    SWQT_FROM = 276,
    SWQT_AS = 277,
    SWQT_ASC = 278,
    SWQT_DESC = 279,
    SWQT_DISTINCT = 280,
    SWQT_CAST = 281,
    SWQT_UNION = 282,
    SWQT_ALL = 283,
    SWQT_LIMIT = 284,
    SWQT_OFFSET = 285,
    SWQT_VALUE_START = 286,
    SWQT_SELECT_START = 287,
    SWQT_NOT = 288,
    SWQT_OR = 289,
    SWQT_AND = 290,
    SWQT_UMINUS = 291,
    SWQT_RESERVED_KEYWORD = 292
};
#endif

/* Value type.  */
#if !defined YYSTYPE && !defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
#define YYSTYPE_IS_TRIVIAL 1
#define YYSTYPE_IS_DECLARED 1
#endif

int swqparse(swq_parse_context *context);

#endif /* !YY_SWQ_SWQ_PARSER_HPP_INCLUDED  */

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
typedef yytype_uint8 yy_state_t;

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
#define YYFINAL 20
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST 401

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS 51
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS 22
/* YYNRULES -- Number of rules.  */
#define YYNRULES 95
/* YYNSTATES -- Number of states.  */
#define YYNSTATES 201

#define YYUNDEFTOK 2
#define YYMAXUTOK 292

/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                                       \
    (0 <= (YYX) && (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] = {
    0,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  39, 2,  2,  2,  44,
    2,  2,  47, 48, 42, 40, 49, 41, 50, 43, 2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  37, 36, 38, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35, 45, 46};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] = {
    0,   122, 122, 123, 129, 136, 141, 146, 151, 158, 166, 174, 182, 190,
    198, 206, 214, 222, 230, 238, 250, 259, 272, 280, 292, 301, 314, 323,
    336, 345, 358, 365, 377, 383, 390, 398, 411, 416, 421, 425, 430, 435,
    440, 475, 482, 489, 496, 503, 510, 546, 554, 560, 567, 576, 594, 614,
    615, 618, 623, 629, 630, 632, 640, 641, 644, 653, 664, 679, 700, 731,
    766, 791, 820, 826, 828, 829, 834, 835, 841, 848, 849, 852, 853, 856,
    862, 868, 875, 876, 883, 884, 892, 902, 913, 924, 937, 948};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 1
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] = {"\"end of string\"",
                                      "error",
                                      "$undefined",
                                      "\"integer number\"",
                                      "\"floating point number\"",
                                      "\"string\"",
                                      "\"identifier\"",
                                      "\"IN\"",
                                      "\"LIKE\"",
                                      "\"ILIKE\"",
                                      "\"ESCAPE\"",
                                      "\"BETWEEN\"",
                                      "\"NULL\"",
                                      "\"IS\"",
                                      "\"SELECT\"",
                                      "\"LEFT\"",
                                      "\"JOIN\"",
                                      "\"WHERE\"",
                                      "\"ON\"",
                                      "\"ORDER\"",
                                      "\"BY\"",
                                      "\"FROM\"",
                                      "\"AS\"",
                                      "\"ASC\"",
                                      "\"DESC\"",
                                      "\"DISTINCT\"",
                                      "\"CAST\"",
                                      "\"UNION\"",
                                      "\"ALL\"",
                                      "\"LIMIT\"",
                                      "\"OFFSET\"",
                                      "SWQT_VALUE_START",
                                      "SWQT_SELECT_START",
                                      "\"NOT\"",
                                      "\"OR\"",
                                      "\"AND\"",
                                      "'='",
                                      "'<'",
                                      "'>'",
                                      "'!'",
                                      "'+'",
                                      "'-'",
                                      "'*'",
                                      "'/'",
                                      "'%'",
                                      "SWQT_UMINUS",
                                      "\"reserved keyword\"",
                                      "'('",
                                      "')'",
                                      "','",
                                      "'.'",
                                      "$accept",
                                      "input",
                                      "value_expr",
                                      "value_expr_list",
                                      "field_value",
                                      "value_expr_non_logical",
                                      "type_def",
                                      "select_statement",
                                      "select_core",
                                      "opt_union_all",
                                      "union_all",
                                      "select_field_list",
                                      "column_spec",
                                      "as_clause",
                                      "opt_where",
                                      "opt_joins",
                                      "opt_order_by",
                                      "sort_spec_list",
                                      "sort_spec",
                                      "opt_limit",
                                      "opt_offset",
                                      "table_def",
                                      YY_NULLPTR};
#endif

#ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_int16 yytoknum[] = {
    0,   256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267,
    268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280,
    281, 282, 283, 284, 285, 286, 287, 288, 289, 290, 61,  60,  62,
    33,  43,  45,  42,  47,  37,  291, 292, 40,  41,  44,  46};
#endif

#define YYPACT_NINF (-133)

#define yypact_value_is_default(Yyn) ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) 0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] = {
    -19,  197,  -7,   1,    -133, -133, -133, -42,  -133, -28,  197,  207,
    197,  329,  -133, 184,  77,   11,   -133, 2,    -133, 197,  25,   197,
    362,  -133, 261,  -8,   197,  197,  207,  9,    249,  197,  197,  89,
    102,  157,  36,   207,  207,  207,  207,  207,  -27,  173,  -133, 279,
    56,   30,   38,   60,   -133, -7,   228,  42,   -133, 313,  -133, 197,
    88,   94,   104,  -133, 99,   73,   197,  197,  207,  346,  242,  197,
    197,  -133, 197,  197,  -133, 197,  -133, 197,  20,   20,   -133, -133,
    -133, 147,  -4,   100,  -133, 117,  -133, 63,   173,  2,    -133, -133,
    197,  -133, 119,  81,   197,  197,  207,  -133, 197,  123,  124,  351,
    -133, -133, -133, -133, -133, -133, 131,  106,  -133, 63,   -133, 92,
    4,    69,   -133, -133, -133, 108,  110,  -133, -133, -133, 184,  116,
    197,  197,  207,  115,  120,  8,    69,   150,  161,  -133, 154,  63,
    164,  54,   -133, -133, -133, -133, 184,  8,    -133, 164,  8,    8,
    63,   153,  197,  155,  48,   64,   -133, 155,  -133, -133, 166,  197,
    329,  162,  158,  -133, 183,  -133, 188,  158,  197,  295,  131,  189,
    165,  148,  149,  165,  295,  -133, 93,   -133, 156,  -133, 204,  -133,
    -133, -133, -133, -133, -133, -133, 131,  -133, -133};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] = {
    2,  0,  0,  0,  36, 37, 38, 34, 41, 0,  0,  0,  0,  3,  39, 5,  0,  0,  4,
    59, 1,  0,  0,  0,  8,  42, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  34, 0,  66, 64, 0,  62, 0,  0,  55, 0,  33, 0,  35,
    0,  40, 0,  18, 22, 0,  30, 0,  0,  0,  0,  0,  7,  6,  0,  0,  9,  0,  0,
    12, 0,  13, 0,  43, 44, 45, 46, 47, 0,  0,  0,  73, 0,  65, 0,  0,  59, 61,
    60, 0,  48, 0,  0,  0,  0,  0,  31, 0,  19, 23, 0,  15, 16, 14, 10, 17, 11,
    0,  0,  67, 0,  72, 0,  90, 76, 63, 56, 32, 50, 0,  26, 20, 24, 28, 0,  0,
    0,  0,  34, 0,  68, 76, 0,  0,  91, 0,  0,  74, 0,  49, 27, 21, 25, 29, 70,
    69, 74, 92, 94, 0,  0,  0,  79, 0,  0,  71, 79, 93, 95, 0,  0,  75, 0,  86,
    51, 0,  53, 0,  86, 0,  76, 0,  0,  88, 0,  0,  88, 76, 77, 83, 80, 82, 87,
    0,  57, 52, 54, 58, 78, 84, 85, 0,  89, 81};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] = {
    -133, -133, -1,  -43, -111, 7,  -133, 163,  191, 125, -133,
    -41,  -133, -96, 68,  -132, 59, 19,   -133, 57,  46,  -100};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] = {-1,  3,   54,  55,  14,  15, 126, 18,
                                         19,  52,  53,  48,  49,  90, 159, 144,
                                         170, 187, 188, 180, 191, 121};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint8 yytable[] = {
    13,  20,  56,  136, 87,  21,  153, 16,  22,  24,  88,  26,  1,   2,   88,
    47,  99,  138, 25,  23,  85,  63,  57,  86,  141, 16,  89,  60,  61,  51,
    89,  56,  69,  70,  73,  76,  78,  62,  116, 59,  17,  152, 64,  157, 47,
    185, 80,  81,  82,  83,  84,  122, 195, 124, 140, 162, 166, 160, 164, 165,
    161, 131, 41,  42,  43,  105, 106, 186, 119, 120, 108, 109, 79,  110, 111,
    107, 112, 91,  113, 92,  4,   5,   6,   44,  142, 143, 93,  186, 94,  8,
    97,  47,  4,   5,   6,   7,   171, 172, 100, 128, 129, 8,   45,  9,   101,
    4,   5,   6,   7,   130, 10,  103, 173, 174, 8,   9,   196, 197, 11,  46,
    104, 117, 10,  118, 12,  125, 71,  72,  9,   127, 11,  148, 149, 132, 133,
    10,  12,  135, 74,  102, 75,  150, 139, 11,  39,  40,  41,  42,  43,  12,
    4,   5,   6,   7,   137, 145, 154, 168, 146, 8,   4,   5,   6,   7,   147,
    22,  177, 155, 151, 8,   156, 167, 114, 9,   169, 184, 4,   5,   6,   44,
    10,  158, 178, 9,   176, 8,   181, 179, 11,  115, 10,  182, 189, 77,  12,
    190, 192, 193, 11,  9,   4,   5,   6,   7,   12,  198, 10,  199, 50,  8,
    4,   5,   6,   7,   11,  46,  95,  200, 123, 8,   12,  163, 175, 9,   39,
    40,  41,  42,  43,  194, 10,  0,   183, 9,   0,   27,  28,  29,  11,  30,
    0,   31,  0,   0,   12,  0,   0,   0,   11,  27,  28,  29,  0,   30,  12,
    31,  65,  66,  67,  0,   68,  32,  33,  34,  35,  36,  37,  38,  27,  28,
    29,  0,   30,  0,   31,  32,  0,   96,  35,  36,  37,  38,  0,   0,   0,
    88,  27,  28,  29,  0,   30,  0,   31,  0,   32,  33,  34,  35,  36,  37,
    38,  89,  27,  28,  29,  0,   30,  0,   31,  58,  142, 143, 32,  33,  34,
    35,  36,  37,  38,  0,   27,  28,  29,  0,   30,  0,   31,  0,   32,  33,
    34,  35,  36,  37,  38,  98,  27,  28,  29,  0,   30,  0,   31,  0,   0,
    0,   32,  33,  34,  35,  36,  37,  38,  27,  28,  29,  0,   30,  0,   31,
    0,   0,   32,  33,  34,  35,  36,  37,  38,  27,  28,  29,  0,   30,  0,
    31,  0,   0,   0,   32,  0,   34,  35,  36,  37,  38,  134, 0,   0,   0,
    0,   39,  40,  41,  42,  43,  0,   0,   35,  36,  37,  38};

static const yytype_int16 yycheck[] = {
    1,  0,   6,  114, 45,  47,  138, 14,  50,  10,  6,   12,  31, 32,  6,   16,
    59, 117, 11, 47,  47,  12,  23,  50,  120, 14,  22,  28,  29, 27,  22,  6,
    33, 34,  35, 36,  37,  30,  42,  47,  47,  137, 33,  143, 45, 177, 39,  40,
    41, 42,  43, 92,  184, 96,  50,  151, 156, 3,   154, 155, 6,  104, 42,  43,
    44, 66,  67, 178, 5,   6,   71,  72,  36,  74,  75,  68,  77, 21,  79,  49,
    3,  4,   5,  6,   15,  16,  48,  198, 28,  12,  48,  92,  3,  4,   5,   6,
    48, 49,  10, 100, 101, 12,  25,  26,  10,  3,   4,   5,   6,  102, 33,  12,
    48, 49,  12, 26,  23,  24,  41,  42,  47,  21,  33,  6,   47, 6,   37,  38,
    26, 48,  41, 132, 133, 10,  10,  33,  47,  6,   36,  35,  38, 134, 50,  41,
    40, 41,  42, 43,  44,  47,  3,   4,   5,   6,   48,  47,  6,  158, 48,  12,
    3,  4,   5,  6,   48,  50,  167, 6,   48,  12,  16,  18,  25, 26,  19,  176,
    3,  4,   5,  6,   33,  17,  20,  26,  18,  12,  3,   29,  41, 42,  33,  3,
    3,  36,  47, 30,  48,  48,  41,  26,  3,   4,   5,   6,   47, 49,  33,  3,
    17, 12,  3,  4,   5,   6,   41,  42,  53,  198, 93,  12,  47, 153, 163, 26,
    40, 41,  42, 43,  44,  183, 33,  -1,  175, 26,  -1,  7,   8,  9,   41,  11,
    -1, 13,  -1, -1,  47,  -1,  -1,  -1,  41,  7,   8,   9,   -1, 11,  47,  13,
    7,  8,   9,  -1,  11,  33,  34,  35,  36,  37,  38,  39,  7,  8,   9,   -1,
    11, -1,  13, 33,  -1,  49,  36,  37,  38,  39,  -1,  -1,  -1, 6,   7,   8,
    9,  -1,  11, -1,  13,  -1,  33,  34,  35,  36,  37,  38,  39, 22,  7,   8,
    9,  -1,  11, -1,  13,  48,  15,  16,  33,  34,  35,  36,  37, 38,  39,  -1,
    7,  8,   9,  -1,  11,  -1,  13,  -1,  33,  34,  35,  36,  37, 38,  39,  22,
    7,  8,   9,  -1,  11,  -1,  13,  -1,  -1,  -1,  33,  34,  35, 36,  37,  38,
    39, 7,   8,  9,   -1,  11,  -1,  13,  -1,  -1,  33,  34,  35, 36,  37,  38,
    39, 7,   8,  9,   -1,  11,  -1,  13,  -1,  -1,  -1,  33,  -1, 35,  36,  37,
    38, 39,  35, -1,  -1,  -1,  -1,  40,  41,  42,  43,  44,  -1, -1,  36,  37,
    38, 39};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_int8 yystos[] = {
    0,  31, 32, 52, 3,  4,  5,  6,  12, 26, 33, 41, 47, 53, 55, 56, 14, 47, 58,
    59, 0,  47, 50, 47, 53, 56, 53, 7,  8,  9,  11, 13, 33, 34, 35, 36, 37, 38,
    39, 40, 41, 42, 43, 44, 6,  25, 42, 53, 62, 63, 59, 27, 60, 61, 53, 54, 6,
    53, 48, 47, 53, 53, 56, 12, 33, 7,  8,  9,  11, 53, 53, 37, 38, 53, 36, 38,
    53, 36, 53, 36, 56, 56, 56, 56, 56, 47, 50, 62, 6,  22, 64, 21, 49, 48, 28,
    58, 49, 48, 22, 54, 10, 10, 35, 12, 47, 53, 53, 56, 53, 53, 53, 53, 53, 53,
    25, 42, 42, 21, 6,  5,  6,  72, 62, 60, 54, 6,  57, 48, 53, 53, 56, 54, 10,
    10, 35, 6,  55, 48, 72, 50, 50, 64, 15, 16, 66, 47, 48, 48, 53, 53, 56, 48,
    64, 66, 6,  6,  16, 72, 17, 65, 3,  6,  64, 65, 64, 64, 72, 18, 53, 19, 67,
    48, 49, 48, 49, 67, 18, 53, 20, 29, 70, 3,  3,  70, 53, 66, 55, 68, 69, 3,
    30, 71, 48, 48, 71, 66, 23, 24, 49, 3,  68};

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_int8 yyr1[] = {
    0,  51, 52, 52, 52, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
    54, 54, 55, 55, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56,
    56, 56, 57, 57, 57, 57, 57, 58, 58, 59, 59, 60, 60, 61, 62, 62,
    63, 63, 63, 63, 63, 63, 63, 63, 64, 64, 65, 65, 66, 66, 66, 67,
    67, 68, 68, 69, 69, 69, 70, 70, 71, 71, 72, 72, 72, 72, 72, 72};

/* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_int8 yyr2[] = {
    0, 2, 0, 2, 2, 1, 3, 3, 2, 3, 4,  4, 3, 3, 4, 4, 4, 4, 3, 4, 5, 6, 3, 4,
    5, 6, 5, 6, 5, 6, 3, 4, 3, 1, 1,  3, 1, 1, 1, 1, 3, 1, 2, 3, 3, 3, 3, 3,
    4, 6, 1, 4, 6, 4, 6, 2, 4, 9, 10, 0, 2, 2, 1, 3, 1, 2, 1, 3, 4, 5, 5, 6,
    2, 1, 0, 2, 0, 5, 6, 0, 3, 3, 1,  1, 2, 2, 0, 2, 0, 2, 1, 2, 3, 4, 3, 4};

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
                                  swq_parse_context *context)
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
                            swq_parse_context *context)
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
                            swq_parse_context *context)
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
                       swq_parse_context *context)
{
    YYUSE(yyvaluep);
    YYUSE(context);
    if (!yymsg)
        yymsg = "Deleting";
    YY_SYMBOL_PRINT(yymsg, yytype, yyvaluep, yylocationp);

    YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
    switch (yytype)
    {
        case 3: /* "integer number"  */
        {
            delete (*yyvaluep);
        }
        break;

        case 4: /* "floating point number"  */
        {
            delete (*yyvaluep);
        }
        break;

        case 5: /* "string"  */
        {
            delete (*yyvaluep);
        }
        break;

        case 6: /* "identifier"  */
        {
            delete (*yyvaluep);
        }
        break;

        case 53: /* value_expr  */
        {
            delete (*yyvaluep);
        }
        break;

        case 54: /* value_expr_list  */
        {
            delete (*yyvaluep);
        }
        break;

        case 55: /* field_value  */
        {
            delete (*yyvaluep);
        }
        break;

        case 56: /* value_expr_non_logical  */
        {
            delete (*yyvaluep);
        }
        break;

        case 57: /* type_def  */
        {
            delete (*yyvaluep);
        }
        break;

        case 72: /* table_def  */
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

int yyparse(swq_parse_context *context)
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
        case 3:
        {
            context->poRoot = yyvsp[0];
            swq_fixup(context);
        }
        break;

        case 4:
        {
            context->poRoot = yyvsp[0];
            swq_fixup(context);
        }
        break;

        case 5:
        {
            yyval = yyvsp[0];
        }
        break;

        case 6:
        {
            yyval = swq_create_and_or_or(SWQ_AND, yyvsp[-2], yyvsp[0]);
        }
        break;

        case 7:
        {
            yyval = swq_create_and_or_or(SWQ_OR, yyvsp[-2], yyvsp[0]);
        }
        break;

        case 8:
        {
            yyval = new swq_expr_node(SWQ_NOT);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 9:
        {
            yyval = new swq_expr_node(SWQ_EQ);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 10:
        {
            yyval = new swq_expr_node(SWQ_NE);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 11:
        {
            yyval = new swq_expr_node(SWQ_NE);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 12:
        {
            yyval = new swq_expr_node(SWQ_LT);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 13:
        {
            yyval = new swq_expr_node(SWQ_GT);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 14:
        {
            yyval = new swq_expr_node(SWQ_LE);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 15:
        {
            yyval = new swq_expr_node(SWQ_LE);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 16:
        {
            yyval = new swq_expr_node(SWQ_LE);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 17:
        {
            yyval = new swq_expr_node(SWQ_GE);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 18:
        {
            yyval = new swq_expr_node(SWQ_LIKE);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 19:
        {
            swq_expr_node *like = new swq_expr_node(SWQ_LIKE);
            like->field_type = SWQ_BOOLEAN;
            like->PushSubExpression(yyvsp[-3]);
            like->PushSubExpression(yyvsp[0]);

            yyval = new swq_expr_node(SWQ_NOT);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(like);
        }
        break;

        case 20:
        {
            yyval = new swq_expr_node(SWQ_LIKE);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-4]);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 21:
        {
            swq_expr_node *like = new swq_expr_node(SWQ_LIKE);
            like->field_type = SWQ_BOOLEAN;
            like->PushSubExpression(yyvsp[-5]);
            like->PushSubExpression(yyvsp[-2]);
            like->PushSubExpression(yyvsp[0]);

            yyval = new swq_expr_node(SWQ_NOT);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(like);
        }
        break;

        case 22:
        {
            yyval = new swq_expr_node(SWQ_ILIKE);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 23:
        {
            swq_expr_node *like = new swq_expr_node(SWQ_ILIKE);
            like->field_type = SWQ_BOOLEAN;
            like->PushSubExpression(yyvsp[-3]);
            like->PushSubExpression(yyvsp[0]);

            yyval = new swq_expr_node(SWQ_NOT);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(like);
        }
        break;

        case 24:
        {
            yyval = new swq_expr_node(SWQ_ILIKE);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-4]);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 25:
        {
            swq_expr_node *like = new swq_expr_node(SWQ_ILIKE);
            like->field_type = SWQ_BOOLEAN;
            like->PushSubExpression(yyvsp[-5]);
            like->PushSubExpression(yyvsp[-2]);
            like->PushSubExpression(yyvsp[0]);

            yyval = new swq_expr_node(SWQ_NOT);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(like);
        }
        break;

        case 26:
        {
            yyval = yyvsp[-1];
            yyval->field_type = SWQ_BOOLEAN;
            yyval->nOperation = SWQ_IN;
            yyval->PushSubExpression(yyvsp[-4]);
            yyval->ReverseSubExpressions();
        }
        break;

        case 27:
        {
            swq_expr_node *in = yyvsp[-1];
            in->field_type = SWQ_BOOLEAN;
            in->nOperation = SWQ_IN;
            in->PushSubExpression(yyvsp[-5]);
            in->ReverseSubExpressions();

            yyval = new swq_expr_node(SWQ_NOT);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(in);
        }
        break;

        case 28:
        {
            yyval = new swq_expr_node(SWQ_BETWEEN);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-4]);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 29:
        {
            swq_expr_node *between = new swq_expr_node(SWQ_BETWEEN);
            between->field_type = SWQ_BOOLEAN;
            between->PushSubExpression(yyvsp[-5]);
            between->PushSubExpression(yyvsp[-2]);
            between->PushSubExpression(yyvsp[0]);

            yyval = new swq_expr_node(SWQ_NOT);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(between);
        }
        break;

        case 30:
        {
            yyval = new swq_expr_node(SWQ_ISNULL);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-2]);
        }
        break;

        case 31:
        {
            swq_expr_node *isnull = new swq_expr_node(SWQ_ISNULL);
            isnull->field_type = SWQ_BOOLEAN;
            isnull->PushSubExpression(yyvsp[-3]);

            yyval = new swq_expr_node(SWQ_NOT);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(isnull);
        }
        break;

        case 32:
        {
            yyval = yyvsp[0];
            yyvsp[0]->PushSubExpression(yyvsp[-2]);
        }
        break;

        case 33:
        {
            yyval = new swq_expr_node(SWQ_ARGUMENT_LIST); /* temporary value */
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 34:
        {
            yyval = yyvsp[0];  // validation deferred.
            yyval->eNodeType = SNT_COLUMN;
            yyval->field_index = -1;
            yyval->table_index = -1;
        }
        break;

        case 35:
        {
            yyval = yyvsp[-2];  // validation deferred.
            yyval->eNodeType = SNT_COLUMN;
            yyval->field_index = -1;
            yyval->table_index = -1;
            yyval->table_name = yyval->string_value;
            yyval->string_value = CPLStrdup(yyvsp[0]->string_value);
            delete yyvsp[0];
            yyvsp[0] = nullptr;
        }
        break;

        case 36:
        {
            yyval = yyvsp[0];
        }
        break;

        case 37:
        {
            yyval = yyvsp[0];
        }
        break;

        case 38:
        {
            yyval = yyvsp[0];
        }
        break;

        case 39:
        {
            yyval = yyvsp[0];
        }
        break;

        case 40:
        {
            yyval = yyvsp[-1];
        }
        break;

        case 41:
        {
            yyval = new swq_expr_node(static_cast<const char *>(nullptr));
        }
        break;

        case 42:
        {
            if (yyvsp[0]->eNodeType == SNT_CONSTANT)
            {
                if (yyvsp[0]->field_type == SWQ_FLOAT &&
                    yyvsp[0]->string_value &&
                    strcmp(yyvsp[0]->string_value, "9223372036854775808") == 0)
                {
                    yyval = yyvsp[0];
                    yyval->field_type = SWQ_INTEGER64;
                    yyval->int_value = std::numeric_limits<GIntBig>::min();
                    yyval->float_value = static_cast<double>(
                        std::numeric_limits<GIntBig>::min());
                }
                // - (-9223372036854775808) cannot be represented on int64
                // the classic overflow is that its negation is itself.
                else if (yyvsp[0]->field_type == SWQ_INTEGER64 &&
                         yyvsp[0]->int_value ==
                             std::numeric_limits<GIntBig>::min())
                {
                    yyval = yyvsp[0];
                }
                else
                {
                    yyval = yyvsp[0];
                    yyval->int_value *= -1;
                    yyval->float_value *= -1;
                }
            }
            else
            {
                yyval = new swq_expr_node(SWQ_MULTIPLY);
                yyval->PushSubExpression(new swq_expr_node(-1));
                yyval->PushSubExpression(yyvsp[0]);
            }
        }
        break;

        case 43:
        {
            yyval = new swq_expr_node(SWQ_ADD);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 44:
        {
            yyval = new swq_expr_node(SWQ_SUBTRACT);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 45:
        {
            yyval = new swq_expr_node(SWQ_MULTIPLY);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 46:
        {
            yyval = new swq_expr_node(SWQ_DIVIDE);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 47:
        {
            yyval = new swq_expr_node(SWQ_MODULUS);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 48:
        {
            const swq_operation *poOp =
                swq_op_registrar::GetOperator(yyvsp[-3]->string_value);

            if (poOp == nullptr)
            {
                if (context->bAcceptCustomFuncs)
                {
                    yyval = yyvsp[-1];
                    yyval->eNodeType = SNT_OPERATION;
                    yyval->nOperation = SWQ_CUSTOM_FUNC;
                    yyval->string_value = CPLStrdup(yyvsp[-3]->string_value);
                    yyval->ReverseSubExpressions();
                    delete yyvsp[-3];
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Undefined function '%s' used.",
                             yyvsp[-3]->string_value);
                    delete yyvsp[-3];
                    delete yyvsp[-1];
                    YYERROR;
                }
            }
            else
            {
                yyval = yyvsp[-1];
                yyval->eNodeType = SNT_OPERATION;
                yyval->nOperation = poOp->eOperation;
                yyval->ReverseSubExpressions();
                delete yyvsp[-3];
            }
        }
        break;

        case 49:
        {
            yyval = yyvsp[-1];
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->ReverseSubExpressions();
        }
        break;

        case 50:
        {
            yyval = new swq_expr_node(SWQ_CAST);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 51:
        {
            yyval = new swq_expr_node(SWQ_CAST);
            yyval->PushSubExpression(yyvsp[-1]);
            yyval->PushSubExpression(yyvsp[-3]);
        }
        break;

        case 52:
        {
            yyval = new swq_expr_node(SWQ_CAST);
            yyval->PushSubExpression(yyvsp[-1]);
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[-5]);
        }
        break;

        case 53:
        {
            OGRwkbGeometryType eType =
                OGRFromOGCGeomType(yyvsp[-1]->string_value);
            if (!EQUAL(yyvsp[-3]->string_value, "GEOMETRY") ||
                (wkbFlatten(eType) == wkbUnknown &&
                 !STARTS_WITH_CI(yyvsp[-1]->string_value, "GEOMETRY")))
            {
                yyerror(context, "syntax error");
                delete yyvsp[-3];
                delete yyvsp[-1];
                YYERROR;
            }
            yyval = new swq_expr_node(SWQ_CAST);
            yyval->PushSubExpression(yyvsp[-1]);
            yyval->PushSubExpression(yyvsp[-3]);
        }
        break;

        case 54:
        {
            OGRwkbGeometryType eType =
                OGRFromOGCGeomType(yyvsp[-3]->string_value);
            if (!EQUAL(yyvsp[-5]->string_value, "GEOMETRY") ||
                (wkbFlatten(eType) == wkbUnknown &&
                 !STARTS_WITH_CI(yyvsp[-3]->string_value, "GEOMETRY")))
            {
                yyerror(context, "syntax error");
                delete yyvsp[-5];
                delete yyvsp[-3];
                delete yyvsp[-1];
                YYERROR;
            }
            yyval = new swq_expr_node(SWQ_CAST);
            yyval->PushSubExpression(yyvsp[-1]);
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[-5]);
        }
        break;

        case 57:
        {
            delete yyvsp[-5];
        }
        break;

        case 58:
        {
            context->poCurSelect->query_mode = SWQM_DISTINCT_LIST;
            delete yyvsp[-5];
        }
        break;

        case 61:
        {
            swq_select *poNewSelect = new swq_select();
            context->poCurSelect->PushUnionAll(poNewSelect);
            context->poCurSelect = poNewSelect;
        }
        break;

        case 64:
        {
            if (!context->poCurSelect->PushField(yyvsp[0]))
            {
                delete yyvsp[0];
                YYERROR;
            }
        }
        break;

        case 65:
        {
            if (!context->poCurSelect->PushField(yyvsp[-1],
                                                 yyvsp[0]->string_value))
            {
                delete yyvsp[-1];
                delete yyvsp[0];
                YYERROR;
            }
            delete yyvsp[0];
        }
        break;

        case 66:
        {
            swq_expr_node *poNode = new swq_expr_node();
            poNode->eNodeType = SNT_COLUMN;
            poNode->string_value = CPLStrdup("*");
            poNode->table_index = -1;
            poNode->field_index = -1;

            if (!context->poCurSelect->PushField(poNode))
            {
                delete poNode;
                YYERROR;
            }
        }
        break;

        case 67:
        {
            CPLString osTableName = yyvsp[-2]->string_value;

            delete yyvsp[-2];
            yyvsp[-2] = nullptr;

            swq_expr_node *poNode = new swq_expr_node();
            poNode->eNodeType = SNT_COLUMN;
            poNode->table_name = CPLStrdup(osTableName);
            poNode->string_value = CPLStrdup("*");
            poNode->table_index = -1;
            poNode->field_index = -1;

            if (!context->poCurSelect->PushField(poNode))
            {
                delete poNode;
                YYERROR;
            }
        }
        break;

        case 68:
        {
            // special case for COUNT(*), confirm it.
            if (!EQUAL(yyvsp[-3]->string_value, "COUNT"))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Syntax Error with %s(*).", yyvsp[-3]->string_value);
                delete yyvsp[-3];
                YYERROR;
            }

            delete yyvsp[-3];
            yyvsp[-3] = nullptr;

            swq_expr_node *poNode = new swq_expr_node();
            poNode->eNodeType = SNT_COLUMN;
            poNode->string_value = CPLStrdup("*");
            poNode->table_index = -1;
            poNode->field_index = -1;

            swq_expr_node *count = new swq_expr_node(SWQ_COUNT);
            count->PushSubExpression(poNode);

            if (!context->poCurSelect->PushField(count))
            {
                delete count;
                YYERROR;
            }
        }
        break;

        case 69:
        {
            // special case for COUNT(*), confirm it.
            if (!EQUAL(yyvsp[-4]->string_value, "COUNT"))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Syntax Error with %s(*).", yyvsp[-4]->string_value);
                delete yyvsp[-4];
                delete yyvsp[0];
                YYERROR;
            }

            delete yyvsp[-4];
            yyvsp[-4] = nullptr;

            swq_expr_node *poNode = new swq_expr_node();
            poNode->eNodeType = SNT_COLUMN;
            poNode->string_value = CPLStrdup("*");
            poNode->table_index = -1;
            poNode->field_index = -1;

            swq_expr_node *count = new swq_expr_node(SWQ_COUNT);
            count->PushSubExpression(poNode);

            if (!context->poCurSelect->PushField(count, yyvsp[0]->string_value))
            {
                delete count;
                delete yyvsp[0];
                YYERROR;
            }

            delete yyvsp[0];
        }
        break;

        case 70:
        {
            // special case for COUNT(DISTINCT x), confirm it.
            if (!EQUAL(yyvsp[-4]->string_value, "COUNT"))
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "DISTINCT keyword can only be used in COUNT() operator.");
                delete yyvsp[-4];
                delete yyvsp[-1];
                YYERROR;
            }

            delete yyvsp[-4];

            swq_expr_node *count = new swq_expr_node(SWQ_COUNT);
            count->PushSubExpression(yyvsp[-1]);

            if (!context->poCurSelect->PushField(count, nullptr, TRUE))
            {
                delete count;
                YYERROR;
            }
        }
        break;

        case 71:
        {
            // special case for COUNT(DISTINCT x), confirm it.
            if (!EQUAL(yyvsp[-5]->string_value, "COUNT"))
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "DISTINCT keyword can only be used in COUNT() operator.");
                delete yyvsp[-5];
                delete yyvsp[-2];
                delete yyvsp[0];
                YYERROR;
            }

            swq_expr_node *count = new swq_expr_node(SWQ_COUNT);
            count->PushSubExpression(yyvsp[-2]);

            if (!context->poCurSelect->PushField(count, yyvsp[0]->string_value,
                                                 TRUE))
            {
                delete yyvsp[-5];
                delete count;
                delete yyvsp[0];
                YYERROR;
            }

            delete yyvsp[-5];
            delete yyvsp[0];
        }
        break;

        case 72:
        {
            delete yyvsp[-1];
            yyval = yyvsp[0];
        }
        break;

        case 75:
        {
            context->poCurSelect->where_expr = yyvsp[0];
        }
        break;

        case 77:
        {
            context->poCurSelect->PushJoin(
                static_cast<int>(yyvsp[-3]->int_value), yyvsp[-1]);
            delete yyvsp[-3];
        }
        break;

        case 78:
        {
            context->poCurSelect->PushJoin(
                static_cast<int>(yyvsp[-3]->int_value), yyvsp[-1]);
            delete yyvsp[-3];
        }
        break;

        case 83:
        {
            context->poCurSelect->PushOrderBy(yyvsp[0]->table_name,
                                              yyvsp[0]->string_value, TRUE);
            delete yyvsp[0];
            yyvsp[0] = nullptr;
        }
        break;

        case 84:
        {
            context->poCurSelect->PushOrderBy(yyvsp[-1]->table_name,
                                              yyvsp[-1]->string_value, TRUE);
            delete yyvsp[-1];
            yyvsp[-1] = nullptr;
        }
        break;

        case 85:
        {
            context->poCurSelect->PushOrderBy(yyvsp[-1]->table_name,
                                              yyvsp[-1]->string_value, FALSE);
            delete yyvsp[-1];
            yyvsp[-1] = nullptr;
        }
        break;

        case 87:
        {
            context->poCurSelect->SetLimit(yyvsp[0]->int_value);
            delete yyvsp[0];
            yyvsp[0] = nullptr;
        }
        break;

        case 89:
        {
            context->poCurSelect->SetOffset(yyvsp[0]->int_value);
            delete yyvsp[0];
            yyvsp[0] = nullptr;
        }
        break;

        case 90:
        {
            const int iTable = context->poCurSelect->PushTableDef(
                nullptr, yyvsp[0]->string_value, nullptr);
            delete yyvsp[0];

            yyval = new swq_expr_node(iTable);
        }
        break;

        case 91:
        {
            const int iTable = context->poCurSelect->PushTableDef(
                nullptr, yyvsp[-1]->string_value, yyvsp[0]->string_value);
            delete yyvsp[-1];
            delete yyvsp[0];

            yyval = new swq_expr_node(iTable);
        }
        break;

        case 92:
        {
            const int iTable = context->poCurSelect->PushTableDef(
                yyvsp[-2]->string_value, yyvsp[0]->string_value, nullptr);
            delete yyvsp[-2];
            delete yyvsp[0];

            yyval = new swq_expr_node(iTable);
        }
        break;

        case 93:
        {
            const int iTable = context->poCurSelect->PushTableDef(
                yyvsp[-3]->string_value, yyvsp[-1]->string_value,
                yyvsp[0]->string_value);
            delete yyvsp[-3];
            delete yyvsp[-1];
            delete yyvsp[0];

            yyval = new swq_expr_node(iTable);
        }
        break;

        case 94:
        {
            const int iTable = context->poCurSelect->PushTableDef(
                yyvsp[-2]->string_value, yyvsp[0]->string_value, nullptr);
            delete yyvsp[-2];
            delete yyvsp[0];

            yyval = new swq_expr_node(iTable);
        }
        break;

        case 95:
        {
            const int iTable = context->poCurSelect->PushTableDef(
                yyvsp[-3]->string_value, yyvsp[-1]->string_value,
                yyvsp[0]->string_value);
            delete yyvsp[-3];
            delete yyvsp[-1];
            delete yyvsp[0];

            yyval = new swq_expr_node(iTable);
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
yyerrorlab:
    /* Pacify compilers when the user code never invokes YYERROR and the
       label yyerrorlab therefore never appears in user code.  */
    if (0)
        YYERROR;

    /* Do not reclaim the symbols of the rule whose action triggered
       this YYERROR.  */
    YYPOPSTACK(yylen);
    yylen = 0;
    YY_STACK_PRINT(yyss, yyssp);
    yystate = *yyssp;
    goto yyerrlab1;

/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
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
