#include <stdio.h>
#include "swq.h"

void swq_expr_dump( swq_expr *expr, FILE * fp, int depth );

char *field_list[] = 
   { "IFIELD", "SFIELD", "FFIELD", "UFIELD", "Messy - Name"};
swq_field_type field_types[] = 
   { SWQ_INTEGER, SWQ_STRING, SWQ_FLOAT, SWQ_OTHER, SWQ_STRING };

static void process_statement( const char *statement );

/************************************************************************/
/*                                main()                                */
/************************************************************************/
int main( int argc, char **argv )

{
    int   iStatement;
    char *statements[] = {
        "SELECT IFIELD, SFIELD FROM TABNAME WHERE IFIELD < 10 ORDER BY IFIELD",
        "SELECT Count(*), MIN(FFIELD), MAX(FFIELD) FROM Provinces",
        NULL };

    if( argc < 2 )
    {
        for( iStatement = 0; statements[iStatement] != NULL; iStatement++ )
            process_statement( statements[iStatement] );
    }
    else
    {
        process_statement( argv[1] );
    }
}

/************************************************************************/
/*                         process_statement()                          */
/************************************************************************/

static void process_statement( const char *statement )

{
    swq_select *select_info;
    const char *error;
    int i;

    printf( "STATEMENT: %s\n", statement );

    error = swq_select_preparse( statement, &select_info );
        
    if( error != NULL )
    {
        fprintf( stderr, "PREPARSE: %s\n", error );
        return;
    }

    error = swq_select_parse( select_info,
                              sizeof(field_list) / sizeof(char*), 
                              field_list, field_types, 0 );
        
    if( error != NULL )
    {
        swq_select_free( select_info );
        fprintf( stderr, "PARSE: %s\n", error );
        return;
    }

    swq_reform_command( select_info );
    printf( "REFORMED: %s\n", select_info->raw_select );

    for( i = 0; i < select_info->result_columns; i++ )
    {
        printf( "  Col %d: ", i+1 );
        if( select_info->column_defs[i].col_func_name != NULL )
            printf( "%s:%d(%s%s:%d)\n", 
                    select_info->column_defs[i].col_func_name,
                    select_info->column_defs[i].col_func,
                    select_info->column_defs[i].distinct_flag 
                    ? "DISTINCT " : "",
                    select_info->column_defs[i].field_name,
                    select_info->column_defs[i].field_index );
        else
            printf( "%s:%d\n", 
                    select_info->column_defs[i].field_name,
                    select_info->column_defs[i].field_index );
    }

    printf( "  FROM table %s\n", select_info->from_table );

    if( select_info->where_expr != NULL )
        ;
    else if( select_info->whole_where_clause != NULL )
        printf( "  WHERE: %s\n", select_info->whole_where_clause );

    if( select_info->order_specs > 0 )
    {
        printf( "  ORDER BY: " );
        for( i = 0; i < select_info->order_specs; i++ )
        {
            printf( "%s:%d ", 
                    select_info->order_defs[i].field_name, 
                    select_info->order_defs[i].field_index );
            if( select_info->order_defs[i].ascending_flag )
                printf( "ASC " );
            else
                printf( "DESC " );
        }

        printf( "\n" );
    }
        
    swq_select_free( select_info );
}
