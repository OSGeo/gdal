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
#define yyparse         ods_formulaparse
#define yylex           ods_formulalex
#define yyerror         ods_formulaerror
#define yydebug         ods_formuladebug
#define yynerrs         ods_formulanerrs

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
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ods_formula.h"


#define YYSTYPE  ods_formula_node*

/* Defining YYSTYPE_IS_TRIVIAL is needed because the parser is generated as a C++ file. */
/* See http://www.gnu.org/s/bison/manual/html_node/Memory-Management.html that suggests */
/* increase YYINITDEPTH instead, but this will consume memory. */
/* Setting YYSTYPE_IS_TRIVIAL overcomes this limitation, but might be fragile because */
/* it appears to be a non documented feature of Bison */
#define YYSTYPE_IS_TRIVIAL 1

static void ods_formulaerror( ods_formula_parse_context * /* context */,
                              const char *msg )
{
    CPLError( CE_Failure, CPLE_AppDefined,
              "Formula Parsing Error: %s", msg );
}



# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "ods_formula_parser.hpp"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_ODST_NUMBER = 3,                /* ODST_NUMBER  */
  YYSYMBOL_ODST_STRING = 4,                /* ODST_STRING  */
  YYSYMBOL_ODST_IDENTIFIER = 5,            /* ODST_IDENTIFIER  */
  YYSYMBOL_ODST_FUNCTION_NO_ARG = 6,       /* ODST_FUNCTION_NO_ARG  */
  YYSYMBOL_ODST_FUNCTION_SINGLE_ARG = 7,   /* ODST_FUNCTION_SINGLE_ARG  */
  YYSYMBOL_ODST_FUNCTION_TWO_ARG = 8,      /* ODST_FUNCTION_TWO_ARG  */
  YYSYMBOL_ODST_FUNCTION_THREE_ARG = 9,    /* ODST_FUNCTION_THREE_ARG  */
  YYSYMBOL_ODST_FUNCTION_ARG_LIST = 10,    /* ODST_FUNCTION_ARG_LIST  */
  YYSYMBOL_ODST_START = 11,                /* ODST_START  */
  YYSYMBOL_ODST_NOT = 12,                  /* ODST_NOT  */
  YYSYMBOL_ODST_OR = 13,                   /* ODST_OR  */
  YYSYMBOL_ODST_AND = 14,                  /* ODST_AND  */
  YYSYMBOL_ODST_IF = 15,                   /* ODST_IF  */
  YYSYMBOL_16_ = 16,                       /* '+'  */
  YYSYMBOL_17_ = 17,                       /* '-'  */
  YYSYMBOL_18_ = 18,                       /* '&'  */
  YYSYMBOL_19_ = 19,                       /* '*'  */
  YYSYMBOL_20_ = 20,                       /* '/'  */
  YYSYMBOL_21_ = 21,                       /* '%'  */
  YYSYMBOL_ODST_UMINUS = 22,               /* ODST_UMINUS  */
  YYSYMBOL_23_ = 23,                       /* ','  */
  YYSYMBOL_24_ = 24,                       /* ';'  */
  YYSYMBOL_25_ = 25,                       /* '('  */
  YYSYMBOL_26_ = 26,                       /* ')'  */
  YYSYMBOL_27_ = 27,                       /* '='  */
  YYSYMBOL_28_ = 28,                       /* '<'  */
  YYSYMBOL_29_ = 29,                       /* '>'  */
  YYSYMBOL_30_ = 30,                       /* '!'  */
  YYSYMBOL_31_ = 31,                       /* '['  */
  YYSYMBOL_32_ = 32,                       /* ']'  */
  YYSYMBOL_33_ = 33,                       /* ':'  */
  YYSYMBOL_YYACCEPT = 34,                  /* $accept  */
  YYSYMBOL_input = 35,                     /* input  */
  YYSYMBOL_comma = 36,                     /* comma  */
  YYSYMBOL_value_expr = 37,                /* value_expr  */
  YYSYMBOL_value_expr_list = 38,           /* value_expr_list  */
  YYSYMBOL_value_expr_and_cell_range_list = 39, /* value_expr_and_cell_range_list  */
  YYSYMBOL_cell_range = 40                 /* cell_range  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
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
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  18
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   333

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  34
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  7
/* YYNRULES -- Number of rules.  */
#define YYNRULES  41
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  108

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   271


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    30,     2,     2,     2,    21,    18,     2,
      25,    26,    19,    16,    23,    17,     2,    20,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    33,    24,
      28,    27,    29,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    31,     2,    32,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    22
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,    73,    73,    78,    78,    82,    87,    92,    97,   103,
     110,   118,   125,   132,   138,   145,   153,   160,   165,   172,
     179,   186,   193,   200,   207,   214,   221,   228,   246,   253,
     260,   267,   274,   281,   288,   295,   301,   308,   314,   319,
     325,   332
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "ODST_NUMBER",
  "ODST_STRING", "ODST_IDENTIFIER", "ODST_FUNCTION_NO_ARG",
  "ODST_FUNCTION_SINGLE_ARG", "ODST_FUNCTION_TWO_ARG",
  "ODST_FUNCTION_THREE_ARG", "ODST_FUNCTION_ARG_LIST", "ODST_START",
  "ODST_NOT", "ODST_OR", "ODST_AND", "ODST_IF", "'+'", "'-'", "'&'", "'*'",
  "'/'", "'%'", "ODST_UMINUS", "','", "';'", "'('", "')'", "'='", "'<'",
  "'>'", "'!'", "'['", "']'", "':'", "$accept", "input", "comma",
  "value_expr", "value_expr_list", "value_expr_and_cell_range_list",
  "cell_range", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-75)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      -4,   162,    12,   -75,   -75,    -3,     4,    14,    21,    32,
      33,    34,    35,    38,   162,   162,    44,   303,   -75,    40,
     162,   162,   162,   182,   162,   162,   162,   162,   -12,   213,
      36,   162,   162,   162,   162,   162,   162,    78,   107,   136,
      43,   -75,   228,    24,    24,    66,    24,    46,   -14,   243,
      24,    49,    50,    24,   -75,   -75,   181,   181,   181,   -12,
     -12,   -12,   162,   162,   303,   162,   162,   303,   162,   303,
     162,   -75,   -75,   -75,   162,   162,    -5,   182,   -75,   182,
     -75,   162,   -75,   -75,   162,   303,   303,   303,   303,   303,
     303,   258,    24,    72,   -75,   -75,   -75,   198,   -75,   162,
      47,   -75,   162,   273,   -75,   288,   -75,   -75
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] =
{
       0,     0,     0,     5,     6,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     2,     1,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    27,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     7,     0,     0,     0,     0,    38,     0,    40,     0,
      36,     0,     0,     0,    17,    34,    28,    29,    30,    31,
      32,    33,     0,     0,    18,     0,     0,    21,     0,    22,
       0,     8,     3,     4,     0,     0,     0,     0,    16,     0,
      13,     0,    12,    11,     0,    24,    25,    23,    19,    26,
      20,     0,     0,     0,    37,    39,    35,     0,     9,     0,
       0,    14,     0,     0,    41,     0,    10,    15
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -75,   -75,   -42,    -1,   -25,   -74,   -75
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
       0,     2,    74,    46,    51,    47,    48
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int8 yytable[] =
{
      17,    52,    75,    94,    77,    95,    79,     1,    81,    72,
      73,    84,    18,    28,    29,    37,    38,    39,    40,    42,
      43,    44,    19,    49,    50,    50,    53,    55,    93,    20,
      56,    57,    58,    59,    60,    61,    64,    67,    69,    21,
      31,    32,    33,    34,    35,    36,    22,    72,    73,    30,
      99,    37,    38,    39,    40,   102,    96,    23,    24,    25,
      26,    85,    86,    27,    87,    88,    41,    89,    55,    90,
      70,    76,    78,    91,    92,    82,    83,   100,     0,   104,
      50,     3,     4,    97,     5,     6,     7,     8,     9,     0,
      10,    11,    12,    13,     0,    14,     0,     0,   103,     0,
       0,   105,     0,    15,     0,     0,    62,    63,     0,    16,
       3,     4,     0,     5,     6,     7,     8,     9,     0,    10,
      11,    12,    13,     0,    14,     0,     0,     0,     0,     0,
       0,     0,    15,     0,    65,     0,    66,     0,    16,     3,
       4,     0,     5,     6,     7,     8,     9,     0,    10,    11,
      12,    13,     0,    14,     0,     0,     0,     0,     0,     0,
       0,    15,     0,    68,     0,     3,     4,    16,     5,     6,
       7,     8,     9,     0,    10,    11,    12,    13,     0,    14,
       0,     0,     0,     0,     0,     3,     4,    15,     5,     6,
       7,     8,     9,    16,    10,    11,    12,    13,     0,    14,
      34,    35,    36,     0,     0,     0,     0,    15,    37,    38,
      39,    40,     0,    45,    31,    32,    33,    34,    35,    36,
       0,    72,    73,     0,   101,    37,    38,    39,    40,    31,
      32,    33,    34,    35,    36,     0,     0,     0,     0,    54,
      37,    38,    39,    40,    31,    32,    33,    34,    35,    36,
       0,     0,     0,     0,    71,    37,    38,    39,    40,    31,
      32,    33,    34,    35,    36,     0,     0,     0,     0,    80,
      37,    38,    39,    40,    31,    32,    33,    34,    35,    36,
       0,     0,     0,     0,    98,    37,    38,    39,    40,    31,
      32,    33,    34,    35,    36,     0,     0,     0,     0,   106,
      37,    38,    39,    40,    31,    32,    33,    34,    35,    36,
       0,     0,     0,     0,   107,    37,    38,    39,    40,    31,
      32,    33,    34,    35,    36,     0,     0,     0,     0,     0,
      37,    38,    39,    40
};

static const yytype_int8 yycheck[] =
{
       1,    26,    44,    77,    46,    79,    48,    11,    50,    23,
      24,    53,     0,    14,    15,    27,    28,    29,    30,    20,
      21,    22,    25,    24,    25,    26,    27,    32,    33,    25,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    25,
      16,    17,    18,    19,    20,    21,    25,    23,    24,     5,
      92,    27,    28,    29,    30,    97,    81,    25,    25,    25,
      25,    62,    63,    25,    65,    66,    26,    68,    32,    70,
      27,     5,    26,    74,    75,    26,    26,     5,    -1,    32,
      81,     3,     4,    84,     6,     7,     8,     9,    10,    -1,
      12,    13,    14,    15,    -1,    17,    -1,    -1,    99,    -1,
      -1,   102,    -1,    25,    -1,    -1,    28,    29,    -1,    31,
       3,     4,    -1,     6,     7,     8,     9,    10,    -1,    12,
      13,    14,    15,    -1,    17,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    25,    -1,    27,    -1,    29,    -1,    31,     3,
       4,    -1,     6,     7,     8,     9,    10,    -1,    12,    13,
      14,    15,    -1,    17,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    25,    -1,    27,    -1,     3,     4,    31,     6,     7,
       8,     9,    10,    -1,    12,    13,    14,    15,    -1,    17,
      -1,    -1,    -1,    -1,    -1,     3,     4,    25,     6,     7,
       8,     9,    10,    31,    12,    13,    14,    15,    -1,    17,
      19,    20,    21,    -1,    -1,    -1,    -1,    25,    27,    28,
      29,    30,    -1,    31,    16,    17,    18,    19,    20,    21,
      -1,    23,    24,    -1,    26,    27,    28,    29,    30,    16,
      17,    18,    19,    20,    21,    -1,    -1,    -1,    -1,    26,
      27,    28,    29,    30,    16,    17,    18,    19,    20,    21,
      -1,    -1,    -1,    -1,    26,    27,    28,    29,    30,    16,
      17,    18,    19,    20,    21,    -1,    -1,    -1,    -1,    26,
      27,    28,    29,    30,    16,    17,    18,    19,    20,    21,
      -1,    -1,    -1,    -1,    26,    27,    28,    29,    30,    16,
      17,    18,    19,    20,    21,    -1,    -1,    -1,    -1,    26,
      27,    28,    29,    30,    16,    17,    18,    19,    20,    21,
      -1,    -1,    -1,    -1,    26,    27,    28,    29,    30,    16,
      17,    18,    19,    20,    21,    -1,    -1,    -1,    -1,    -1,
      27,    28,    29,    30
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,    11,    35,     3,     4,     6,     7,     8,     9,    10,
      12,    13,    14,    15,    17,    25,    31,    37,     0,    25,
      25,    25,    25,    25,    25,    25,    25,    25,    37,    37,
       5,    16,    17,    18,    19,    20,    21,    27,    28,    29,
      30,    26,    37,    37,    37,    31,    37,    39,    40,    37,
      37,    38,    38,    37,    26,    32,    37,    37,    37,    37,
      37,    37,    28,    29,    37,    27,    29,    37,    27,    37,
      27,    26,    23,    24,    36,    36,     5,    36,    26,    36,
      26,    36,    26,    26,    36,    37,    37,    37,    37,    37,
      37,    37,    37,    33,    39,    39,    38,    37,    26,    36,
       5,    26,    36,    37,    32,    37,    26,    26
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    34,    35,    36,    36,    37,    37,    37,    37,    37,
      37,    37,    37,    37,    37,    37,    37,    37,    37,    37,
      37,    37,    37,    37,    37,    37,    37,    37,    37,    37,
      37,    37,    37,    37,    37,    38,    38,    39,    39,    39,
      39,    40
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     2,     1,     1,     1,     1,     3,     4,     6,
       8,     4,     4,     4,     6,     8,     4,     3,     3,     4,
       4,     3,     3,     4,     4,     4,     4,     2,     3,     3,
       3,     3,     3,     3,     3,     3,     1,     3,     1,     3,
       1,     5
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (context, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, context); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, ods_formula_parse_context *context)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (context);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, ods_formula_parse_context *context)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep, context);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule, ods_formula_parse_context *context)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)], context);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule, context); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, ods_formula_parse_context *context)
{
  YY_USE (yyvaluep);
  YY_USE (context);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  switch (yykind)
    {
    case YYSYMBOL_ODST_NUMBER: /* ODST_NUMBER  */
            { delete (*yyvaluep); }
        break;

    case YYSYMBOL_ODST_STRING: /* ODST_STRING  */
            { delete (*yyvaluep); }
        break;

    case YYSYMBOL_ODST_IDENTIFIER: /* ODST_IDENTIFIER  */
            { delete (*yyvaluep); }
        break;

    case YYSYMBOL_ODST_FUNCTION_NO_ARG: /* ODST_FUNCTION_NO_ARG  */
            { delete (*yyvaluep); }
        break;

    case YYSYMBOL_ODST_FUNCTION_SINGLE_ARG: /* ODST_FUNCTION_SINGLE_ARG  */
            { delete (*yyvaluep); }
        break;

    case YYSYMBOL_ODST_FUNCTION_TWO_ARG: /* ODST_FUNCTION_TWO_ARG  */
            { delete (*yyvaluep); }
        break;

    case YYSYMBOL_ODST_FUNCTION_THREE_ARG: /* ODST_FUNCTION_THREE_ARG  */
            { delete (*yyvaluep); }
        break;

    case YYSYMBOL_ODST_FUNCTION_ARG_LIST: /* ODST_FUNCTION_ARG_LIST  */
            { delete (*yyvaluep); }
        break;

    case YYSYMBOL_value_expr: /* value_expr  */
            { delete (*yyvaluep); }
        break;

    case YYSYMBOL_value_expr_list: /* value_expr_list  */
            { delete (*yyvaluep); }
        break;

    case YYSYMBOL_value_expr_and_cell_range_list: /* value_expr_and_cell_range_list  */
            { delete (*yyvaluep); }
        break;

    case YYSYMBOL_cell_range: /* cell_range  */
            { delete (*yyvaluep); }
        break;

      default:
        break;
    }
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}






/*----------.
| yyparse.  |
`----------*/

int
yyparse (ods_formula_parse_context *context)
{
/* Lookahead token kind.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

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



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

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
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
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
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
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
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex (&yylval, context);
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
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
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
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
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* input: ODST_START value_expr  */
        {
            context->poRoot = yyvsp[0];
        }
    break;

  case 5: /* value_expr: ODST_NUMBER  */
        {
            yyval = yyvsp[0];
        }
    break;

  case 6: /* value_expr: ODST_STRING  */
        {
            yyval = yyvsp[0];
        }
    break;

  case 7: /* value_expr: ODST_FUNCTION_NO_ARG '(' ')'  */
        {
            yyval = yyvsp[-2];
        }
    break;

  case 8: /* value_expr: ODST_FUNCTION_SINGLE_ARG '(' value_expr ')'  */
        {
            yyval = yyvsp[-3];
            yyval->PushSubExpression( yyvsp[-1] );
        }
    break;

  case 9: /* value_expr: ODST_FUNCTION_TWO_ARG '(' value_expr comma value_expr ')'  */
        {
            yyval = yyvsp[-5];
            yyval->PushSubExpression( yyvsp[-3] );
            yyval->PushSubExpression( yyvsp[-1] );
        }
    break;

  case 10: /* value_expr: ODST_FUNCTION_THREE_ARG '(' value_expr comma value_expr comma value_expr ')'  */
        {
            yyval = yyvsp[-7];
            yyval->PushSubExpression( yyvsp[-5] );
            yyval->PushSubExpression( yyvsp[-3] );
            yyval->PushSubExpression( yyvsp[-1] );
        }
    break;

  case 11: /* value_expr: ODST_AND '(' value_expr_list ')'  */
        {
            yyval = new ods_formula_node( ODS_AND );
            yyvsp[-1]->ReverseSubExpressions();
            yyval->PushSubExpression( yyvsp[-1] );
        }
    break;

  case 12: /* value_expr: ODST_OR '(' value_expr_list ')'  */
        {
            yyval = new ods_formula_node( ODS_OR );
            yyvsp[-1]->ReverseSubExpressions();
            yyval->PushSubExpression( yyvsp[-1] );
        }
    break;

  case 13: /* value_expr: ODST_NOT '(' value_expr ')'  */
        {
            yyval = new ods_formula_node( ODS_NOT );
            yyval->PushSubExpression( yyvsp[-1] );
        }
    break;

  case 14: /* value_expr: ODST_IF '(' value_expr comma value_expr ')'  */
        {
            yyval = new ods_formula_node( ODS_IF );
            yyval->PushSubExpression( yyvsp[-3] );
            yyval->PushSubExpression( yyvsp[-1] );
        }
    break;

  case 15: /* value_expr: ODST_IF '(' value_expr comma value_expr comma value_expr ')'  */
        {
            yyval = new ods_formula_node( ODS_IF );
            yyval->PushSubExpression( yyvsp[-5] );
            yyval->PushSubExpression( yyvsp[-3] );
            yyval->PushSubExpression( yyvsp[-1] );
        }
    break;

  case 16: /* value_expr: ODST_FUNCTION_ARG_LIST '(' value_expr_and_cell_range_list ')'  */
        {
            yyval = yyvsp[-3];
            yyvsp[-1]->ReverseSubExpressions();
            yyval->PushSubExpression( yyvsp[-1] );
        }
    break;

  case 17: /* value_expr: '(' value_expr ')'  */
        {
            yyval = yyvsp[-1];
        }
    break;

  case 18: /* value_expr: value_expr '=' value_expr  */
        {
            yyval = new ods_formula_node( ODS_EQ );
            yyval->PushSubExpression( yyvsp[-2] );
            yyval->PushSubExpression( yyvsp[0] );
        }
    break;

  case 19: /* value_expr: value_expr '<' '>' value_expr  */
        {
            yyval = new ods_formula_node( ODS_NE );
            yyval->PushSubExpression( yyvsp[-3] );
            yyval->PushSubExpression( yyvsp[0] );
        }
    break;

  case 20: /* value_expr: value_expr '!' '=' value_expr  */
        {
            yyval = new ods_formula_node( ODS_NE );
            yyval->PushSubExpression( yyvsp[-3] );
            yyval->PushSubExpression( yyvsp[0] );
        }
    break;

  case 21: /* value_expr: value_expr '<' value_expr  */
        {
            yyval = new ods_formula_node( ODS_LT );
            yyval->PushSubExpression( yyvsp[-2] );
            yyval->PushSubExpression( yyvsp[0] );
        }
    break;

  case 22: /* value_expr: value_expr '>' value_expr  */
        {
            yyval = new ods_formula_node( ODS_GT );
            yyval->PushSubExpression( yyvsp[-2] );
            yyval->PushSubExpression( yyvsp[0] );
        }
    break;

  case 23: /* value_expr: value_expr '<' '=' value_expr  */
        {
            yyval = new ods_formula_node( ODS_LE );
            yyval->PushSubExpression( yyvsp[-3] );
            yyval->PushSubExpression( yyvsp[0] );
        }
    break;

  case 24: /* value_expr: value_expr '=' '<' value_expr  */
        {
            yyval = new ods_formula_node( ODS_LE );
            yyval->PushSubExpression( yyvsp[-3] );
            yyval->PushSubExpression( yyvsp[0] );
        }
    break;

  case 25: /* value_expr: value_expr '=' '>' value_expr  */
        {
            yyval = new ods_formula_node( ODS_LE );
            yyval->PushSubExpression( yyvsp[-3] );
            yyval->PushSubExpression( yyvsp[0] );
        }
    break;

  case 26: /* value_expr: value_expr '>' '=' value_expr  */
        {
            yyval = new ods_formula_node( ODS_GE );
            yyval->PushSubExpression( yyvsp[-3] );
            yyval->PushSubExpression( yyvsp[0] );
        }
    break;

  case 27: /* value_expr: '-' value_expr  */
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
                yyval = new ods_formula_node( ODS_MULTIPLY );
                yyval->PushSubExpression( new ods_formula_node(-1) );
                yyval->PushSubExpression( yyvsp[0] );
            }
        }
    break;

  case 28: /* value_expr: value_expr '+' value_expr  */
        {
            yyval = new ods_formula_node( ODS_ADD );
            yyval->PushSubExpression( yyvsp[-2] );
            yyval->PushSubExpression( yyvsp[0] );
        }
    break;

  case 29: /* value_expr: value_expr '-' value_expr  */
        {
            yyval = new ods_formula_node( ODS_SUBTRACT );
            yyval->PushSubExpression( yyvsp[-2] );
            yyval->PushSubExpression( yyvsp[0] );
        }
    break;

  case 30: /* value_expr: value_expr '&' value_expr  */
        {
            yyval = new ods_formula_node( ODS_CONCAT );
            yyval->PushSubExpression( yyvsp[-2] );
            yyval->PushSubExpression( yyvsp[0] );
        }
    break;

  case 31: /* value_expr: value_expr '*' value_expr  */
        {
            yyval = new ods_formula_node( ODS_MULTIPLY );
            yyval->PushSubExpression( yyvsp[-2] );
            yyval->PushSubExpression( yyvsp[0] );
        }
    break;

  case 32: /* value_expr: value_expr '/' value_expr  */
        {
            yyval = new ods_formula_node( ODS_DIVIDE );
            yyval->PushSubExpression( yyvsp[-2] );
            yyval->PushSubExpression( yyvsp[0] );
        }
    break;

  case 33: /* value_expr: value_expr '%' value_expr  */
        {
            yyval = new ods_formula_node( ODS_MODULUS );
            yyval->PushSubExpression( yyvsp[-2] );
            yyval->PushSubExpression( yyvsp[0] );
        }
    break;

  case 34: /* value_expr: '[' ODST_IDENTIFIER ']'  */
        {
            yyval = new ods_formula_node( ODS_CELL );
            yyval->PushSubExpression( yyvsp[-1] );
        }
    break;

  case 35: /* value_expr_list: value_expr comma value_expr_list  */
        {
            yyval = yyvsp[0];
            yyvsp[0]->PushSubExpression( yyvsp[-2] );
        }
    break;

  case 36: /* value_expr_list: value_expr  */
            {
            yyval = new ods_formula_node( ODS_LIST );
            yyval->PushSubExpression( yyvsp[0] );
        }
    break;

  case 37: /* value_expr_and_cell_range_list: value_expr comma value_expr_and_cell_range_list  */
        {
            yyval = yyvsp[0];
            yyvsp[0]->PushSubExpression( yyvsp[-2] );
        }
    break;

  case 38: /* value_expr_and_cell_range_list: value_expr  */
            {
            yyval = new ods_formula_node( ODS_LIST );
            yyval->PushSubExpression( yyvsp[0] );
        }
    break;

  case 39: /* value_expr_and_cell_range_list: cell_range comma value_expr_and_cell_range_list  */
        {
            yyval = yyvsp[0];
            yyvsp[0]->PushSubExpression( yyvsp[-2] );
        }
    break;

  case 40: /* value_expr_and_cell_range_list: cell_range  */
            {
            yyval = new ods_formula_node( ODS_LIST );
            yyval->PushSubExpression( yyvsp[0] );
        }
    break;

  case 41: /* cell_range: '[' ODST_IDENTIFIER ':' ODST_IDENTIFIER ']'  */
        {
            yyval = new ods_formula_node( ODS_CELL_RANGE );
            yyval->PushSubExpression( yyvsp[-3] );
            yyval->PushSubExpression( yyvsp[-1] );
        }
    break;



      default: break;
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
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
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
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs; (void)yynerrs;
      yyerror (context, YY_("syntax error"));
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
          yydestruct ("Error: discarding",
                      yytoken, &yylval, context);
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
  ++yynerrs; (void)yynerrs;

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
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
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


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, context);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

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
  yyerror (context, YY_("memory exhausted"));
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
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, context);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, context);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

