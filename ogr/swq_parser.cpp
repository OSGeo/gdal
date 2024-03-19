/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

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

/* Defining YYSTYPE_IS_TRIVIAL is needed because the parser is generated as a C++ file. */
/* See http://www.gnu.org/s/bison/manual/html_node/Memory-Management.html that suggests */
/* increase YYINITDEPTH instead, but this will consume memory. */
/* Setting YYSTYPE_IS_TRIVIAL overcomes this limitation, but might be fragile because */
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

#include "swq_parser.hpp"

/* Symbol kind.  */
enum yysymbol_kind_t
{
    YYSYMBOL_YYEMPTY = -2,
    YYSYMBOL_YYEOF = 0,                   /* "end of string"  */
    YYSYMBOL_YYerror = 1,                 /* error  */
    YYSYMBOL_YYUNDEF = 2,                 /* "invalid token"  */
    YYSYMBOL_SWQT_INTEGER_NUMBER = 3,     /* "integer number"  */
    YYSYMBOL_SWQT_FLOAT_NUMBER = 4,       /* "floating point number"  */
    YYSYMBOL_SWQT_STRING = 5,             /* "string"  */
    YYSYMBOL_SWQT_IDENTIFIER = 6,         /* "identifier"  */
    YYSYMBOL_SWQT_IN = 7,                 /* "IN"  */
    YYSYMBOL_SWQT_LIKE = 8,               /* "LIKE"  */
    YYSYMBOL_SWQT_ILIKE = 9,              /* "ILIKE"  */
    YYSYMBOL_SWQT_ESCAPE = 10,            /* "ESCAPE"  */
    YYSYMBOL_SWQT_BETWEEN = 11,           /* "BETWEEN"  */
    YYSYMBOL_SWQT_NULL = 12,              /* "NULL"  */
    YYSYMBOL_SWQT_IS = 13,                /* "IS"  */
    YYSYMBOL_SWQT_SELECT = 14,            /* "SELECT"  */
    YYSYMBOL_SWQT_LEFT = 15,              /* "LEFT"  */
    YYSYMBOL_SWQT_JOIN = 16,              /* "JOIN"  */
    YYSYMBOL_SWQT_WHERE = 17,             /* "WHERE"  */
    YYSYMBOL_SWQT_ON = 18,                /* "ON"  */
    YYSYMBOL_SWQT_ORDER = 19,             /* "ORDER"  */
    YYSYMBOL_SWQT_BY = 20,                /* "BY"  */
    YYSYMBOL_SWQT_FROM = 21,              /* "FROM"  */
    YYSYMBOL_SWQT_AS = 22,                /* "AS"  */
    YYSYMBOL_SWQT_ASC = 23,               /* "ASC"  */
    YYSYMBOL_SWQT_DESC = 24,              /* "DESC"  */
    YYSYMBOL_SWQT_DISTINCT = 25,          /* "DISTINCT"  */
    YYSYMBOL_SWQT_CAST = 26,              /* "CAST"  */
    YYSYMBOL_SWQT_UNION = 27,             /* "UNION"  */
    YYSYMBOL_SWQT_ALL = 28,               /* "ALL"  */
    YYSYMBOL_SWQT_LIMIT = 29,             /* "LIMIT"  */
    YYSYMBOL_SWQT_OFFSET = 30,            /* "OFFSET"  */
    YYSYMBOL_SWQT_EXCEPT = 31,            /* "EXCEPT"  */
    YYSYMBOL_SWQT_EXCLUDE = 32,           /* "EXCLUDE"  */
    YYSYMBOL_SWQT_VALUE_START = 33,       /* SWQT_VALUE_START  */
    YYSYMBOL_SWQT_SELECT_START = 34,      /* SWQT_SELECT_START  */
    YYSYMBOL_SWQT_NOT = 35,               /* "NOT"  */
    YYSYMBOL_SWQT_OR = 36,                /* "OR"  */
    YYSYMBOL_SWQT_AND = 37,               /* "AND"  */
    YYSYMBOL_38_ = 38,                    /* '='  */
    YYSYMBOL_39_ = 39,                    /* '<'  */
    YYSYMBOL_40_ = 40,                    /* '>'  */
    YYSYMBOL_41_ = 41,                    /* '!'  */
    YYSYMBOL_42_ = 42,                    /* '+'  */
    YYSYMBOL_43_ = 43,                    /* '-'  */
    YYSYMBOL_44_ = 44,                    /* '*'  */
    YYSYMBOL_45_ = 45,                    /* '/'  */
    YYSYMBOL_46_ = 46,                    /* '%'  */
    YYSYMBOL_SWQT_UMINUS = 47,            /* SWQT_UMINUS  */
    YYSYMBOL_SWQT_RESERVED_KEYWORD = 48,  /* "reserved keyword"  */
    YYSYMBOL_49_ = 49,                    /* '('  */
    YYSYMBOL_50_ = 50,                    /* ')'  */
    YYSYMBOL_51_ = 51,                    /* ','  */
    YYSYMBOL_52_ = 52,                    /* '.'  */
    YYSYMBOL_YYACCEPT = 53,               /* $accept  */
    YYSYMBOL_input = 54,                  /* input  */
    YYSYMBOL_value_expr = 55,             /* value_expr  */
    YYSYMBOL_value_expr_list = 56,        /* value_expr_list  */
    YYSYMBOL_field_value = 57,            /* field_value  */
    YYSYMBOL_value_expr_non_logical = 58, /* value_expr_non_logical  */
    YYSYMBOL_type_def = 59,               /* type_def  */
    YYSYMBOL_select_statement = 60,       /* select_statement  */
    YYSYMBOL_select_core = 61,            /* select_core  */
    YYSYMBOL_opt_union_all = 62,          /* opt_union_all  */
    YYSYMBOL_union_all = 63,              /* union_all  */
    YYSYMBOL_select_field_list = 64,      /* select_field_list  */
    YYSYMBOL_exclude_field = 65,          /* exclude_field  */
    YYSYMBOL_exclude_field_list = 66,     /* exclude_field_list  */
    YYSYMBOL_except_or_exclude = 67,      /* except_or_exclude  */
    YYSYMBOL_column_spec = 68,            /* column_spec  */
    YYSYMBOL_as_clause = 69,              /* as_clause  */
    YYSYMBOL_opt_where = 70,              /* opt_where  */
    YYSYMBOL_opt_joins = 71,              /* opt_joins  */
    YYSYMBOL_opt_order_by = 72,           /* opt_order_by  */
    YYSYMBOL_sort_spec_list = 73,         /* sort_spec_list  */
    YYSYMBOL_sort_spec = 74,              /* sort_spec  */
    YYSYMBOL_opt_limit = 75,              /* opt_limit  */
    YYSYMBOL_opt_offset = 76,             /* opt_offset  */
    YYSYMBOL_table_def = 77               /* table_def  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;

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

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
#undef UINT_LEAST8_MAX
#undef UINT_LEAST16_MAX
#define UINT_LEAST8_MAX 255
#define UINT_LEAST16_MAX 65535
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
#define YY_USE(E) ((void)(E))
#else
#define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && !defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
#if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                                    \
    _Pragma("GCC diagnostic push")                                             \
        _Pragma("GCC diagnostic ignored \"-Wuninitialized\"")
#else
#define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                                    \
    _Pragma("GCC diagnostic push")                                             \
        _Pragma("GCC diagnostic ignored \"-Wuninitialized\"")                  \
            _Pragma("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
#endif
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

#if 1

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
#endif /* 1 */

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
#define YYLAST 409

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS 53
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS 25
/* YYNRULES -- Number of rules.  */
#define YYNRULES 101
/* YYNSTATES -- Number of states.  */
#define YYNSTATES 211

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK 294

/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                                       \
    (0 <= (YYX) && (YYX) <= YYMAXUTOK                                          \
         ? YY_CAST(yysymbol_kind_t, yytranslate[YYX])                          \
         : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] = {
    0,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  41, 2,  2,  2,  46,
    2,  2,  49, 50, 44, 42, 51, 43, 52, 45, 2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  39, 38, 40, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
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
    30, 31, 32, 33, 34, 35, 36, 37, 47, 48};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] = {
    0,   124, 124, 125, 131, 138, 143, 148, 153, 160, 168, 176, 184, 192, 200,
    208, 216, 224, 232, 240, 252, 261, 274, 282, 294, 303, 316, 325, 338, 347,
    360, 367, 379, 385, 392, 400, 413, 418, 423, 427, 432, 437, 442, 477, 484,
    491, 498, 505, 512, 548, 556, 562, 569, 578, 596, 616, 617, 620, 625, 631,
    632, 634, 642, 643, 646, 656, 657, 660, 661, 664, 673, 684, 699, 714, 735,
    766, 801, 826, 855, 861, 863, 864, 869, 870, 876, 883, 884, 887, 888, 891,
    897, 903, 910, 911, 918, 919, 927, 937, 948, 959, 972, 983};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST(yysymbol_kind_t, yystos[State])

#if 1
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] = {"\"end of string\"",
                                      "error",
                                      "\"invalid token\"",
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
                                      "\"EXCEPT\"",
                                      "\"EXCLUDE\"",
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
                                      "exclude_field",
                                      "exclude_field_list",
                                      "except_or_exclude",
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

#define YYPACT_NINF (-137)

#define yypact_value_is_default(Yyn) ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) 0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] = {
    34,   206,  -10,  16,   -137, -137, -137, -35,  -137, -37,  206,  211,
    206,  326,  -137, 298,  79,   11,   -137, 4,    -137, 206,  23,   206,
    368,  -137, 254,  -11,  206,  206,  211,  -5,   12,   206,  206,  94,
    104,  155,  5,    211,  211,  211,  211,  211,  8,    196,  93,   274,
    48,   26,   30,   65,   -137, -10,  235,  45,   -137, 310,  -137, 206,
    91,   101,  44,   -137, 103,  68,   206,  206,  211,  345,  361,  206,
    206,  -137, 206,  206,  -137, 206,  -137, 206,  18,   18,   -137, -137,
    -137, 145,  -3,   100,  -137, -137, 89,   -137, 140,  -137, 121,  196,
    4,    -137, -137, 206,  -137, 146,  114,  206,  206,  211,  -137, 206,
    144,  158,  182,  -137, -137, -137, -137, -137, -137, 159,  119,  -137,
    121,  159,  -137, 120,  2,    116,  -137, -137, -137, 124,  125,  -137,
    -137, -137, 298,  127,  206,  206,  211,  122,  128,  20,   116,  -137,
    131,  129,  177,  178,  -137, 170,  121,  174,  53,   -137, -137, -137,
    -137, 298,  20,   -137, 174,  159,  -137, 20,   20,   121,  169,  206,
    173,  90,   105,  -137, 173,  -137, -137, -137, 179,  206,  326,  175,
    167,  -137, 200,  -137, 202,  167,  206,  290,  159,  203,  183,  157,
    171,  183,  290,  -137, 139,  -137, 184,  -137, 217,  -137, -137, -137,
    -137, -137, -137, -137, 159,  -137, -137};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] = {
    2,  0,  0,   0,  36, 37, 38, 34, 41, 0,  0,  0,  0,  3,   39, 5,  0,  0,
    4,  59, 1,   0,  0,  0,  8,  42, 0,  0,  0,  0,  0,  0,   0,  0,  0,  0,
    0,  0,  0,   0,  0,  0,  0,  0,  34, 0,  72, 69, 0,  62,  0,  0,  55, 0,
    33, 0,  35,  0,  40, 0,  18, 22, 0,  30, 0,  0,  0,  0,   0,  7,  6,  0,
    0,  9,  0,   0,  12, 0,  13, 0,  43, 44, 45, 46, 47, 0,   0,  0,  67, 68,
    0,  79, 0,   70, 0,  0,  59, 61, 60, 0,  48, 0,  0,  0,   0,  0,  31, 0,
    19, 23, 0,   15, 16, 14, 10, 17, 11, 0,  0,  73, 0,  0,   78, 0,  96, 82,
    63, 56, 32,  50, 0,  26, 20, 24, 28, 0,  0,  0,  0,  34,  0,  74, 82, 64,
    65, 0,  0,   0,  97, 0,  0,  80, 0,  49, 27, 21, 25, 29,  76, 75, 80, 0,
    71, 98, 100, 0,  0,  0,  85, 0,  0,  77, 85, 66, 99, 101, 0,  0,  81, 0,
    92, 51, 0,   53, 0,  92, 0,  82, 0,  0,  94, 0,  0,  94,  82, 83, 89, 86,
    88, 93, 0,   57, 52, 54, 58, 84, 90, 91, 0,  95, 87};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] = {
    -137, -137, -1,   -46, -116, 7,    -137, 176, 213,  137, -137, -43, -137,
    73,   -137, -137, -45, 76,   -136, 66,   39,  -137, 67,  57,   -110};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_uint8 yydefgoto[] = {
    0,   3,  54, 55, 14,  15,  130, 18,  19,  52,  53,  48, 144,
    145, 90, 49, 93, 168, 151, 180, 197, 198, 190, 201, 125};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint8 yytable[] = {
    13,  140, 87,  56,  16,  143, 160, 63,  91,  24,  142, 26,  23,  102, 21,
    47,  20,  22,  25,  65,  66,  67,  57,  68,  92,  16,  91,  60,  61,  56,
    64,  51,  69,  70,  73,  76,  78,  62,  59,  17,  166, 119, 92,  79,  47,
    143, 80,  81,  82,  83,  84,  195, 126, 128, 147, 176, 169, 85,  205, 170,
    86,  135, 41,  42,  43,  108, 109, 1,   2,   94,  111, 112, 196, 113, 114,
    110, 115, 95,  116, 148, 96,  105, 4,   5,   6,   44,  39,  40,  41,  42,
    43,  8,   196, 97,  47,  100, 159, 4,   5,   6,   7,   103, 132, 133, 45,
    9,   8,   4,   5,   6,   7,   104, 134, 171, 10,  106, 8,   107, 174, 175,
    9,   120, 11,  46,  88,  89,  123, 124, 12,  10,  9,   149, 150, 71,  72,
    155, 156, 11,  121, 10,  181, 182, 74,  12,  75,  157, 122, 11,  4,   5,
    6,   7,   129, 12,  136, 183, 184, 8,   4,   5,   6,   7,   206, 207, 131,
    139, 178, 8,   137, 141, 117, 9,   146, 152, 22,  153, 187, 154, 158, 162,
    10,  9,   161, 163, 164, 194, 165, 177, 11,  118, 10,  167, 179, 77,  12,
    188, 189, 186, 11,  4,   5,   6,   44,  191, 12,  192, 199, 202, 8,   4,
    5,   6,   7,   200, 4,   5,   6,   7,   8,   138, 209, 203, 9,   8,   39,
    40,  41,  42,  43,  98,  50,  10,  9,   127, 173, 208, 172, 9,   185, 11,
    46,  10,  27,  28,  29,  12,  30,  210, 31,  11,  204, 0,   193, 0,   11,
    12,  0,   0,   0,   0,   12,  27,  28,  29,  0,   30,  0,   31,  0,   0,
    32,  33,  34,  35,  36,  37,  38,  0,   0,   0,   91,  27,  28,  29,  0,
    30,  99,  31,  0,   32,  33,  34,  35,  36,  37,  38,  92,  27,  28,  29,
    0,   30,  0,   31,  58,  149, 150, 0,   0,   32,  33,  34,  35,  36,  37,
    38,  0,   27,  28,  29,  0,   30,  0,   31,  0,   32,  33,  34,  35,  36,
    37,  38,  101, 27,  28,  29,  0,   30,  0,   31,  39,  40,  41,  42,  43,
    32,  33,  34,  35,  36,  37,  38,  27,  28,  29,  0,   30,  0,   31,  0,
    0,   32,  33,  34,  35,  36,  37,  38,  27,  28,  29,  0,   30,  0,   31,
    27,  28,  29,  0,   30,  32,  31,  34,  35,  36,  37,  38,  0,   0,   0,
    0,   0,   0,   0,   0,   0,   32,  0,   0,   35,  36,  37,  38,  0,   0,
    0,   35,  36,  37,  38};

static const yytype_int16 yycheck[] = {
    1,   117, 45, 6,   14, 121, 142, 12,  6,   10,  120, 12, 49,  59,  49,  16,
    0,   52,  11, 7,   8,  9,   23,  11,  22,  14,  6,   28, 29,  6,   35,  27,
    33,  34,  35, 36,  37, 30,  49,  49,  150, 44,  22,  38, 45,  161, 39,  40,
    41,  42,  43, 187, 95, 99,  52,  165, 3,   49,  194, 6,  52,  107, 44,  45,
    46,  66,  67, 33,  34, 21,  71,  72,  188, 74,  75,  68, 77,  51,  79,  124,
    50,  37,  3,  4,   5,  6,   42,  43,  44,  45,  46,  12, 208, 28,  95,  50,
    141, 3,   4,  5,   6,  10,  103, 104, 25,  26,  12,  3,  4,   5,   6,   10,
    105, 158, 35, 12,  12, 49,  163, 164, 26,  21,  43,  44, 31,  32,  5,   6,
    49,  35,  26, 15,  16, 39,  40,  136, 137, 43,  49,  35, 50,  51,  38,  49,
    40,  138, 6,  43,  3,  4,   5,   6,   6,   49,  10,  50, 51,  12,  3,   4,
    5,   6,   23, 24,  50, 6,   167, 12,  10,  50,  25,  26, 52,  49,  52,  50,
    177, 50,  50, 50,  35, 26,  51,  6,   6,   186, 16,  18, 43,  44,  35,  17,
    19,  38,  49, 20,  29, 18,  43,  3,   4,   5,   6,   3,  49,  3,   3,   50,
    12,  3,   4,  5,   6,  30,  3,   4,   5,   6,   12,  37, 3,   50,  26,  12,
    42,  43,  44, 45,  46, 53,  17,  35,  26,  96,  161, 51, 160, 26,  172, 43,
    44,  35,  7,  8,   9,  49,  11,  208, 13,  43,  193, -1, 185, -1,  43,  49,
    -1,  -1,  -1, -1,  49, 7,   8,   9,   -1,  11,  -1,  13, -1,  -1,  35,  36,
    37,  38,  39, 40,  41, -1,  -1,  -1,  6,   7,   8,   9,  -1,  11,  51,  13,
    -1,  35,  36, 37,  38, 39,  40,  41,  22,  7,   8,   9,  -1,  11,  -1,  13,
    50,  15,  16, -1,  -1, 35,  36,  37,  38,  39,  40,  41, -1,  7,   8,   9,
    -1,  11,  -1, 13,  -1, 35,  36,  37,  38,  39,  40,  41, 22,  7,   8,   9,
    -1,  11,  -1, 13,  42, 43,  44,  45,  46,  35,  36,  37, 38,  39,  40,  41,
    7,   8,   9,  -1,  11, -1,  13,  -1,  -1,  35,  36,  37, 38,  39,  40,  41,
    7,   8,   9,  -1,  11, -1,  13,  7,   8,   9,   -1,  11, 35,  13,  37,  38,
    39,  40,  41, -1,  -1, -1,  -1,  -1,  -1,  -1,  -1,  -1, 35,  -1,  -1,  38,
    39,  40,  41, -1,  -1, -1,  38,  39,  40,  41};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] = {
    0,  33, 34, 54, 3,  4,  5,  6,  12, 26, 35, 43, 49, 55, 57, 58, 14, 49,
    60, 61, 0,  49, 52, 49, 55, 58, 55, 7,  8,  9,  11, 13, 35, 36, 37, 38,
    39, 40, 41, 42, 43, 44, 45, 46, 6,  25, 44, 55, 64, 68, 61, 27, 62, 63,
    55, 56, 6,  55, 50, 49, 55, 55, 58, 12, 35, 7,  8,  9,  11, 55, 55, 39,
    40, 55, 38, 40, 55, 38, 55, 38, 58, 58, 58, 58, 58, 49, 52, 64, 31, 32,
    67, 6,  22, 69, 21, 51, 50, 28, 60, 51, 50, 22, 56, 10, 10, 37, 12, 49,
    55, 55, 58, 55, 55, 55, 55, 55, 55, 25, 44, 44, 21, 49, 6,  5,  6,  77,
    64, 62, 56, 6,  59, 50, 55, 55, 58, 56, 10, 10, 37, 6,  57, 50, 77, 57,
    65, 66, 52, 52, 69, 15, 16, 71, 49, 50, 50, 55, 55, 58, 50, 69, 71, 51,
    50, 6,  6,  16, 77, 17, 70, 3,  6,  69, 70, 66, 69, 69, 77, 18, 55, 19,
    72, 50, 51, 50, 51, 72, 18, 55, 20, 29, 75, 3,  3,  75, 55, 71, 57, 73,
    74, 3,  30, 76, 50, 50, 76, 71, 23, 24, 51, 3,  73};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] = {
    0,  53, 54, 54, 54, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55,
    55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 56, 56,
    57, 57, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 59,
    59, 59, 59, 59, 60, 60, 61, 61, 62, 62, 63, 64, 64, 65, 66, 66, 67,
    67, 68, 68, 68, 68, 68, 68, 68, 68, 68, 69, 69, 70, 70, 71, 71, 71,
    72, 72, 73, 73, 74, 74, 74, 75, 75, 76, 76, 77, 77, 77, 77, 77, 77};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] = {
    0, 2, 0, 2, 2, 1, 3, 3, 2, 3, 4, 4, 3, 3, 4, 4, 4,  4, 3, 4, 5,
    6, 3, 4, 5, 6, 5, 6, 5, 6, 3, 4, 3, 1, 1, 3, 1, 1,  1, 1, 3, 1,
    2, 3, 3, 3, 3, 3, 4, 6, 1, 4, 6, 4, 6, 2, 4, 9, 10, 0, 2, 2, 1,
    3, 1, 1, 3, 1, 1, 1, 2, 5, 1, 3, 4, 5, 5, 6, 2, 1,  0, 2, 0, 5,
    6, 0, 3, 3, 1, 1, 2, 2, 0, 2, 0, 2, 1, 2, 3, 4, 3,  4};

enum
{
    YYENOMEM = -2
};

#define yyerrok (yyerrstatus = 0)
#define yyclearin (yychar = YYEMPTY)

#define YYACCEPT goto yyacceptlab
#define YYABORT goto yyabortlab
#define YYERROR goto yyerrorlab
#define YYNOMEM goto yyexhaustedlab

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

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF

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

#define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                          \
    do                                                                         \
    {                                                                          \
        if (yydebug)                                                           \
        {                                                                      \
            YYFPRINTF(stderr, "%s ", Title);                                   \
            yy_symbol_print(stderr, Kind, Value, context);                     \
            YYFPRINTF(stderr, "\n");                                           \
        }                                                                      \
    } while (0)

/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void yy_symbol_value_print(FILE *yyo, yysymbol_kind_t yykind,
                                  YYSTYPE const *const yyvaluep,
                                  swq_parse_context *context)
{
    FILE *yyoutput = yyo;
    YY_USE(yyoutput);
    YY_USE(context);
    if (!yyvaluep)
        return;
    YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
    YY_USE(yykind);
    YY_IGNORE_MAYBE_UNINITIALIZED_END
}

/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void yy_symbol_print(FILE *yyo, yysymbol_kind_t yykind,
                            YYSTYPE const *const yyvaluep,
                            swq_parse_context *context)
{
    YYFPRINTF(yyo, "%s %s (", yykind < YYNTOKENS ? "token" : "nterm",
              yysymbol_name(yykind));

    yy_symbol_value_print(yyo, yykind, yyvaluep, context);
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
        yy_symbol_print(stderr, YY_ACCESSING_SYMBOL(+yyssp[yyi + 1 - yynrhs]),
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
#define YYDPRINTF(Args) ((void)0)
#define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
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

/* Context of a parse error.  */
typedef struct
{
    yy_state_t *yyssp;
    yysymbol_kind_t yytoken;
} yypcontext_t;

/* Put in YYARG at most YYARGN of the expected tokens given the
   current YYCTX, and return the number of tokens stored in YYARG.  If
   YYARG is null, return the number of expected tokens (guaranteed to
   be less than YYNTOKENS).  Return YYENOMEM on memory exhaustion.
   Return 0 if there are more than YYARGN expected tokens, yet fill
   YYARG up to YYARGN. */
static int yypcontext_expected_tokens(const yypcontext_t *yyctx,
                                      yysymbol_kind_t yyarg[], int yyargn)
{
    /* Actual size of YYARG. */
    int yycount = 0;
    int yyn = yypact[+*yyctx->yyssp];
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
            if (yycheck[yyx + yyn] == yyx && yyx != YYSYMBOL_YYerror &&
                !yytable_value_is_error(yytable[yyx + yyn]))
            {
                if (!yyarg)
                    ++yycount;
                else if (yycount == yyargn)
                    return 0;
                else
                    yyarg[yycount++] = YY_CAST(yysymbol_kind_t, yyx);
            }
    }
    if (yyarg && yycount == 0 && 0 < yyargn)
        yyarg[0] = YYSYMBOL_YYEMPTY;
    return yycount;
}

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

static int yy_syntax_error_arguments(const yypcontext_t *yyctx,
                                     yysymbol_kind_t yyarg[], int yyargn)
{
    /* Actual size of YYARG. */
    int yycount = 0;
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
    if (yyctx->yytoken != YYSYMBOL_YYEMPTY)
    {
        int yyn;
        if (yyarg)
            yyarg[yycount] = yyctx->yytoken;
        ++yycount;
        yyn = yypcontext_expected_tokens(yyctx, yyarg ? yyarg + 1 : yyarg,
                                         yyargn - 1);
        if (yyn == YYENOMEM)
            return YYENOMEM;
        else
            yycount += yyn;
    }
    return yycount;
}

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return -1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return YYENOMEM if the
   required number of bytes is too large to store.  */
static int yysyntax_error(YYPTRDIFF_T *yymsg_alloc, char **yymsg,
                          const yypcontext_t *yyctx)
{
    enum
    {
        YYARGS_MAX = 5
    };

    /* Internationalized format string. */
    const char *yyformat = YY_NULLPTR;
    /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
    yysymbol_kind_t yyarg[YYARGS_MAX];
    /* Cumulated lengths of YYARG.  */
    YYPTRDIFF_T yysize = 0;

    /* Actual size of YYARG. */
    int yycount = yy_syntax_error_arguments(yyctx, yyarg, YYARGS_MAX);
    if (yycount == YYENOMEM)
        return YYENOMEM;

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

    /* Compute error message size.  Don't count the "%s"s, but reserve
     room for the terminator.  */
    yysize = yystrlen(yyformat) - 2 * yycount + 1;
    {
        int yyi;
        for (yyi = 0; yyi < yycount; ++yyi)
        {
            YYPTRDIFF_T yysize1 =
                yysize + yytnamerr(YY_NULLPTR, yytname[yyarg[yyi]]);
            if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
                yysize = yysize1;
            else
                return YYENOMEM;
        }
    }

    if (*yymsg_alloc < yysize)
    {
        *yymsg_alloc = 2 * yysize;
        if (!(yysize <= *yymsg_alloc && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
            *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
        return -1;
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
                yyp += yytnamerr(yyp, yytname[yyarg[yyi++]]);
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

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void yydestruct(const char *yymsg, yysymbol_kind_t yykind,
                       YYSTYPE *yyvaluep, swq_parse_context *context)
{
    YY_USE(yyvaluep);
    YY_USE(context);
    if (!yymsg)
        yymsg = "Deleting";
    YY_SYMBOL_PRINT(yymsg, yykind, yyvaluep, yylocationp);

    YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
    switch (yykind)
    {
        case YYSYMBOL_SWQT_INTEGER_NUMBER: /* "integer number"  */
        {
            delete (*yyvaluep);
        }
        break;

        case YYSYMBOL_SWQT_FLOAT_NUMBER: /* "floating point number"  */
        {
            delete (*yyvaluep);
        }
        break;

        case YYSYMBOL_SWQT_STRING: /* "string"  */
        {
            delete (*yyvaluep);
        }
        break;

        case YYSYMBOL_SWQT_IDENTIFIER: /* "identifier"  */
        {
            delete (*yyvaluep);
        }
        break;

        case YYSYMBOL_value_expr: /* value_expr  */
        {
            delete (*yyvaluep);
        }
        break;

        case YYSYMBOL_value_expr_list: /* value_expr_list  */
        {
            delete (*yyvaluep);
        }
        break;

        case YYSYMBOL_field_value: /* field_value  */
        {
            delete (*yyvaluep);
        }
        break;

        case YYSYMBOL_value_expr_non_logical: /* value_expr_non_logical  */
        {
            delete (*yyvaluep);
        }
        break;

        case YYSYMBOL_type_def: /* type_def  */
        {
            delete (*yyvaluep);
        }
        break;

        case YYSYMBOL_table_def: /* table_def  */
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
    /* Lookahead token kind.  */
    int yychar;

    /* The semantic value of the lookahead symbol.  */
    /* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
    YY_INITIAL_VALUE(static YYSTYPE yyval_default;)
    YYSTYPE yylval YY_INITIAL_VALUE(= yyval_default);

    /* Number of syntax errors so far.  */
    int yynerrs = 0;

    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

    int yyn;
    /* The return value of yyparse.  */
    int yyresult;
    /* Lookahead symbol kind.  */
    yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
    /* The variables used to return semantic value and location from the
     action routines.  */
    YYSTYPE yyval;

    /* Buffer for error messages, and its allocated size.  */
    char yymsgbuf[128];
    char *yymsg = yymsgbuf;
    YYPTRDIFF_T yymsg_alloc = sizeof yymsgbuf;

#define YYPOPSTACK(N) (yyvsp -= (N), yyssp -= (N))

    /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
    int yylen = 0;

    YYDPRINTF((stderr, "Starting parse\n"));

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
    YY_IGNORE_USELESS_CAST_BEGIN
    *yyssp = YY_CAST(yy_state_t, yystate);
    YY_IGNORE_USELESS_CAST_END
    YY_STACK_PRINT(yyss, yyssp);

    if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
        YYNOMEM;
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
            YYNOMEM;
        yystacksize *= 2;
        if (YYMAXDEPTH < yystacksize)
            yystacksize = YYMAXDEPTH;

        {
            yy_state_t *yyss1 = yyss;
            union yyalloc *yyptr = YY_CAST(
                union yyalloc *,
                YYSTACK_ALLOC(YY_CAST(YYSIZE_T, YYSTACK_BYTES(yystacksize))));
            if (!yyptr)
                YYNOMEM;
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

    /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
    if (yychar == YYEMPTY)
    {
        YYDPRINTF((stderr, "Reading a token\n"));
        yychar = yylex(&yylval, context);
    }

    if (yychar <= END)
    {
        yychar = END;
        yytoken = YYSYMBOL_YYEOF;
        YYDPRINTF((stderr, "Now at end of input.\n"));
    }
    else if (yychar == YYerror)
    {
        /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
        yychar = YYUNDEF;
        yytoken = YYSYMBOL_YYerror;
        goto yyerrlab1;
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
        case 3: /* input: SWQT_VALUE_START value_expr  */
        {
            context->poRoot = yyvsp[0];
            swq_fixup(context);
        }
        break;

        case 4: /* input: SWQT_SELECT_START select_statement  */
        {
            context->poRoot = yyvsp[0];
            // swq_fixup() must be done by caller
        }
        break;

        case 5: /* value_expr: value_expr_non_logical  */
        {
            yyval = yyvsp[0];
        }
        break;

        case 6: /* value_expr: value_expr "AND" value_expr  */
        {
            yyval = swq_create_and_or_or(SWQ_AND, yyvsp[-2], yyvsp[0]);
        }
        break;

        case 7: /* value_expr: value_expr "OR" value_expr  */
        {
            yyval = swq_create_and_or_or(SWQ_OR, yyvsp[-2], yyvsp[0]);
        }
        break;

        case 8: /* value_expr: "NOT" value_expr  */
        {
            yyval = new swq_expr_node(SWQ_NOT);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 9: /* value_expr: value_expr '=' value_expr  */
        {
            yyval = new swq_expr_node(SWQ_EQ);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 10: /* value_expr: value_expr '<' '>' value_expr  */
        {
            yyval = new swq_expr_node(SWQ_NE);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 11: /* value_expr: value_expr '!' '=' value_expr  */
        {
            yyval = new swq_expr_node(SWQ_NE);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 12: /* value_expr: value_expr '<' value_expr  */
        {
            yyval = new swq_expr_node(SWQ_LT);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 13: /* value_expr: value_expr '>' value_expr  */
        {
            yyval = new swq_expr_node(SWQ_GT);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 14: /* value_expr: value_expr '<' '=' value_expr  */
        {
            yyval = new swq_expr_node(SWQ_LE);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 15: /* value_expr: value_expr '=' '<' value_expr  */
        {
            yyval = new swq_expr_node(SWQ_LE);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 16: /* value_expr: value_expr '=' '>' value_expr  */
        {
            yyval = new swq_expr_node(SWQ_LE);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 17: /* value_expr: value_expr '>' '=' value_expr  */
        {
            yyval = new swq_expr_node(SWQ_GE);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 18: /* value_expr: value_expr "LIKE" value_expr  */
        {
            yyval = new swq_expr_node(SWQ_LIKE);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 19: /* value_expr: value_expr "NOT" "LIKE" value_expr  */
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

        case 20: /* value_expr: value_expr "LIKE" value_expr "ESCAPE" value_expr  */
        {
            yyval = new swq_expr_node(SWQ_LIKE);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-4]);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 21: /* value_expr: value_expr "NOT" "LIKE" value_expr "ESCAPE" value_expr  */
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

        case 22: /* value_expr: value_expr "ILIKE" value_expr  */
        {
            yyval = new swq_expr_node(SWQ_ILIKE);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 23: /* value_expr: value_expr "NOT" "ILIKE" value_expr  */
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

        case 24: /* value_expr: value_expr "ILIKE" value_expr "ESCAPE" value_expr  */
        {
            yyval = new swq_expr_node(SWQ_ILIKE);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-4]);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 25: /* value_expr: value_expr "NOT" "ILIKE" value_expr "ESCAPE" value_expr  */
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

        case 26: /* value_expr: value_expr "IN" '(' value_expr_list ')'  */
        {
            yyval = yyvsp[-1];
            yyval->field_type = SWQ_BOOLEAN;
            yyval->nOperation = SWQ_IN;
            yyval->PushSubExpression(yyvsp[-4]);
            yyval->ReverseSubExpressions();
        }
        break;

        case 27: /* value_expr: value_expr "NOT" "IN" '(' value_expr_list ')'  */
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

        case 28: /* value_expr: value_expr "BETWEEN" value_expr_non_logical "AND" value_expr_non_logical  */
        {
            yyval = new swq_expr_node(SWQ_BETWEEN);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-4]);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 29: /* value_expr: value_expr "NOT" "BETWEEN" value_expr_non_logical "AND" value_expr_non_logical  */
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

        case 30: /* value_expr: value_expr "IS" "NULL"  */
        {
            yyval = new swq_expr_node(SWQ_ISNULL);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(yyvsp[-2]);
        }
        break;

        case 31: /* value_expr: value_expr "IS" "NOT" "NULL"  */
        {
            swq_expr_node *isnull = new swq_expr_node(SWQ_ISNULL);
            isnull->field_type = SWQ_BOOLEAN;
            isnull->PushSubExpression(yyvsp[-3]);

            yyval = new swq_expr_node(SWQ_NOT);
            yyval->field_type = SWQ_BOOLEAN;
            yyval->PushSubExpression(isnull);
        }
        break;

        case 32: /* value_expr_list: value_expr ',' value_expr_list  */
        {
            yyval = yyvsp[0];
            yyvsp[0]->PushSubExpression(yyvsp[-2]);
        }
        break;

        case 33: /* value_expr_list: value_expr  */
        {
            yyval = new swq_expr_node(SWQ_ARGUMENT_LIST); /* temporary value */
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 34: /* field_value: "identifier"  */
        {
            yyval = yyvsp[0];  // validation deferred.
            yyval->eNodeType = SNT_COLUMN;
            yyval->field_index = -1;
            yyval->table_index = -1;
        }
        break;

        case 35: /* field_value: "identifier" '.' "identifier"  */
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

        case 36: /* value_expr_non_logical: "integer number"  */
        {
            yyval = yyvsp[0];
        }
        break;

        case 37: /* value_expr_non_logical: "floating point number"  */
        {
            yyval = yyvsp[0];
        }
        break;

        case 38: /* value_expr_non_logical: "string"  */
        {
            yyval = yyvsp[0];
        }
        break;

        case 39: /* value_expr_non_logical: field_value  */
        {
            yyval = yyvsp[0];
        }
        break;

        case 40: /* value_expr_non_logical: '(' value_expr ')'  */
        {
            yyval = yyvsp[-1];
        }
        break;

        case 41: /* value_expr_non_logical: "NULL"  */
        {
            yyval = new swq_expr_node(static_cast<const char *>(nullptr));
        }
        break;

        case 42: /* value_expr_non_logical: '-' value_expr_non_logical  */
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

        case 43: /* value_expr_non_logical: value_expr_non_logical '+' value_expr_non_logical  */
        {
            yyval = new swq_expr_node(SWQ_ADD);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 44: /* value_expr_non_logical: value_expr_non_logical '-' value_expr_non_logical  */
        {
            yyval = new swq_expr_node(SWQ_SUBTRACT);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 45: /* value_expr_non_logical: value_expr_non_logical '*' value_expr_non_logical  */
        {
            yyval = new swq_expr_node(SWQ_MULTIPLY);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 46: /* value_expr_non_logical: value_expr_non_logical '/' value_expr_non_logical  */
        {
            yyval = new swq_expr_node(SWQ_DIVIDE);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 47: /* value_expr_non_logical: value_expr_non_logical '%' value_expr_non_logical  */
        {
            yyval = new swq_expr_node(SWQ_MODULUS);
            yyval->PushSubExpression(yyvsp[-2]);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 48: /* value_expr_non_logical: "identifier" '(' value_expr_list ')'  */
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

        case 49: /* value_expr_non_logical: "CAST" '(' value_expr "AS" type_def ')'  */
        {
            yyval = yyvsp[-1];
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->ReverseSubExpressions();
        }
        break;

        case 50: /* type_def: "identifier"  */
        {
            yyval = new swq_expr_node(SWQ_CAST);
            yyval->PushSubExpression(yyvsp[0]);
        }
        break;

        case 51: /* type_def: "identifier" '(' "integer number" ')'  */
        {
            yyval = new swq_expr_node(SWQ_CAST);
            yyval->PushSubExpression(yyvsp[-1]);
            yyval->PushSubExpression(yyvsp[-3]);
        }
        break;

        case 52: /* type_def: "identifier" '(' "integer number" ',' "integer number" ')'  */
        {
            yyval = new swq_expr_node(SWQ_CAST);
            yyval->PushSubExpression(yyvsp[-1]);
            yyval->PushSubExpression(yyvsp[-3]);
            yyval->PushSubExpression(yyvsp[-5]);
        }
        break;

        case 53: /* type_def: "identifier" '(' "identifier" ')'  */
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

        case 54: /* type_def: "identifier" '(' "identifier" ',' "integer number" ')'  */
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

        case 57: /* select_core: "SELECT" select_field_list "FROM" table_def opt_joins opt_where opt_order_by opt_limit opt_offset  */
        {
            delete yyvsp[-5];
        }
        break;

        case 58: /* select_core: "SELECT" "DISTINCT" select_field_list "FROM" table_def opt_joins opt_where opt_order_by opt_limit opt_offset  */
        {
            context->poCurSelect->query_mode = SWQM_DISTINCT_LIST;
            delete yyvsp[-5];
        }
        break;

        case 61: /* union_all: "UNION" "ALL"  */
        {
            swq_select *poNewSelect = new swq_select();
            context->poCurSelect->PushUnionAll(poNewSelect);
            context->poCurSelect = poNewSelect;
        }
        break;

        case 64: /* exclude_field: field_value  */
        {
            if (!context->poCurSelect->PushExcludeField(yyvsp[0]))
            {
                delete yyvsp[0];
                YYERROR;
            }
        }
        break;

        case 69: /* column_spec: value_expr  */
        {
            if (!context->poCurSelect->PushField(yyvsp[0]))
            {
                delete yyvsp[0];
                YYERROR;
            }
        }
        break;

        case 70: /* column_spec: value_expr as_clause  */
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

        case 71: /* column_spec: '*' except_or_exclude '(' exclude_field_list ')'  */
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

        case 72: /* column_spec: '*'  */
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

        case 73: /* column_spec: "identifier" '.' '*'  */
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

        case 74: /* column_spec: "identifier" '(' '*' ')'  */
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

        case 75: /* column_spec: "identifier" '(' '*' ')' as_clause  */
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

        case 76: /* column_spec: "identifier" '(' "DISTINCT" field_value ')'  */
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

        case 77: /* column_spec: "identifier" '(' "DISTINCT" field_value ')' as_clause  */
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

        case 78: /* as_clause: "AS" "identifier"  */
        {
            delete yyvsp[-1];
            yyval = yyvsp[0];
        }
        break;

        case 81: /* opt_where: "WHERE" value_expr  */
        {
            context->poCurSelect->where_expr = yyvsp[0];
        }
        break;

        case 83: /* opt_joins: "JOIN" table_def "ON" value_expr opt_joins  */
        {
            context->poCurSelect->PushJoin(
                static_cast<int>(yyvsp[-3]->int_value), yyvsp[-1]);
            delete yyvsp[-3];
        }
        break;

        case 84: /* opt_joins: "LEFT" "JOIN" table_def "ON" value_expr opt_joins  */
        {
            context->poCurSelect->PushJoin(
                static_cast<int>(yyvsp[-3]->int_value), yyvsp[-1]);
            delete yyvsp[-3];
        }
        break;

        case 89: /* sort_spec: field_value  */
        {
            context->poCurSelect->PushOrderBy(yyvsp[0]->table_name,
                                              yyvsp[0]->string_value, TRUE);
            delete yyvsp[0];
            yyvsp[0] = nullptr;
        }
        break;

        case 90: /* sort_spec: field_value "ASC"  */
        {
            context->poCurSelect->PushOrderBy(yyvsp[-1]->table_name,
                                              yyvsp[-1]->string_value, TRUE);
            delete yyvsp[-1];
            yyvsp[-1] = nullptr;
        }
        break;

        case 91: /* sort_spec: field_value "DESC"  */
        {
            context->poCurSelect->PushOrderBy(yyvsp[-1]->table_name,
                                              yyvsp[-1]->string_value, FALSE);
            delete yyvsp[-1];
            yyvsp[-1] = nullptr;
        }
        break;

        case 93: /* opt_limit: "LIMIT" "integer number"  */
        {
            context->poCurSelect->SetLimit(yyvsp[0]->int_value);
            delete yyvsp[0];
            yyvsp[0] = nullptr;
        }
        break;

        case 95: /* opt_offset: "OFFSET" "integer number"  */
        {
            context->poCurSelect->SetOffset(yyvsp[0]->int_value);
            delete yyvsp[0];
            yyvsp[0] = nullptr;
        }
        break;

        case 96: /* table_def: "identifier"  */
        {
            const int iTable = context->poCurSelect->PushTableDef(
                nullptr, yyvsp[0]->string_value, nullptr);
            delete yyvsp[0];

            yyval = new swq_expr_node(iTable);
        }
        break;

        case 97: /* table_def: "identifier" as_clause  */
        {
            const int iTable = context->poCurSelect->PushTableDef(
                nullptr, yyvsp[-1]->string_value, yyvsp[0]->string_value);
            delete yyvsp[-1];
            delete yyvsp[0];

            yyval = new swq_expr_node(iTable);
        }
        break;

        case 98: /* table_def: "string" '.' "identifier"  */
        {
            const int iTable = context->poCurSelect->PushTableDef(
                yyvsp[-2]->string_value, yyvsp[0]->string_value, nullptr);
            delete yyvsp[-2];
            delete yyvsp[0];

            yyval = new swq_expr_node(iTable);
        }
        break;

        case 99: /* table_def: "string" '.' "identifier" as_clause  */
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

        case 100: /* table_def: "identifier" '.' "identifier"  */
        {
            const int iTable = context->poCurSelect->PushTableDef(
                yyvsp[-2]->string_value, yyvsp[0]->string_value, nullptr);
            delete yyvsp[-2];
            delete yyvsp[0];

            yyval = new swq_expr_node(iTable);
        }
        break;

        case 101: /* table_def: "identifier" '.' "identifier" as_clause  */
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
    YY_SYMBOL_PRINT("-> $$ =", YY_CAST(yysymbol_kind_t, yyr1[yyn]), &yyval,
                    &yyloc);

    YYPOPSTACK(yylen);
    yylen = 0;

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
    yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE(yychar);
    /* If not already recovering from an error, report this error.  */
    if (!yyerrstatus)
    {
        ++yynerrs;
        (void)yynerrs;
        {
            yypcontext_t yyctx = {yyssp, yytoken};
            char const *yymsgp = YY_("syntax error");
            int yysyntax_error_status;
            yysyntax_error_status =
                yysyntax_error(&yymsg_alloc, &yymsg, &yyctx);
            if (yysyntax_error_status == 0)
                yymsgp = yymsg;
            else if (yysyntax_error_status == -1)
            {
                if (yymsg != yymsgbuf)
                    YYSTACK_FREE(yymsg);
                yymsg = YY_CAST(char *,
                                YYSTACK_ALLOC(YY_CAST(YYSIZE_T, yymsg_alloc)));
                if (yymsg)
                {
                    yysyntax_error_status =
                        yysyntax_error(&yymsg_alloc, &yymsg, &yyctx);
                    yymsgp = yymsg;
                }
                else
                {
                    yymsg = yymsgbuf;
                    yymsg_alloc = sizeof yymsgbuf;
                    yysyntax_error_status = YYENOMEM;
                }
            }
            yyerror(context, yymsgp);
            if (yysyntax_error_status == YYENOMEM)
                YYNOMEM;
        }
    }

    if (yyerrstatus == 3)
    {
        /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

        if (yychar <= END)
        {
            /* Return failure if at end of input.  */
            if (yychar == END)
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
    ++yynerrs;
    (void)yynerrs;

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

    /* Pop stack until we find a state that shifts the error token.  */
    for (;;)
    {
        yyn = yypact[yystate];
        if (!yypact_value_is_default(yyn))
        {
            yyn += YYSYMBOL_YYerror;
            if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
                yyn = yytable[yyn];
                if (0 < yyn)
                    break;
            }
        }

        /* Pop the current state because it cannot handle the error token.  */
        if (yyssp == yyss)
            YYABORT;

        yydestruct("Error: popping", YY_ACCESSING_SYMBOL(yystate), yyvsp,
                   context);
        YYPOPSTACK(1);
        yystate = *yyssp;
        YY_STACK_PRINT(yyss, yyssp);
    }

    YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
    *++yyvsp = yylval;
    YY_IGNORE_MAYBE_UNINITIALIZED_END

    /* Shift the error token.  */
    YY_SYMBOL_PRINT("Shifting", YY_ACCESSING_SYMBOL(yyn), yyvsp, yylsp);

    yystate = yyn;
    goto yynewstate;

/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
    yyresult = 0;
    goto yyreturnlab;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
    yyresult = 1;
    goto yyreturnlab;

/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
    yyerror(context, YY_("memory exhausted"));
    yyresult = 2;
    goto yyreturnlab;

/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
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
        yydestruct("Cleanup: popping", YY_ACCESSING_SYMBOL(+*yyssp), yyvsp,
                   context);
        YYPOPSTACK(1);
    }
#ifndef yyoverflow
    if (yyss != yyssa)
        YYSTACK_FREE(yyss);
#endif
    if (yymsg != yymsgbuf)
        YYSTACK_FREE(yymsg);
    return yyresult;
}
