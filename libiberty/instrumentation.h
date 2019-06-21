/*cp-demangle.c (d_expression_1): Fix out-of-bounds access. Check code variable for NULL before dereferencing it.*/
#define C597B1 = 1
/*cp-demangle.c (d_find_pack): Do not recurse for FIXED_TYPE, DEFAULT_ARG and NUMBER.*/
#define C597B2 = 1
/*cp-demangle.c (d_print_comp_inner): Add NULL pointer check.*/
#define C597B3 = 1
/*cp-demangle.h (d_peek_next_char): Define as inline function when CHECK_DEMANGLER is defined. (d_advance): Likewise.*/
#define C597B4 = 1





/*
#if C597B1 == 1
      FILE * inslog;
      inslog = fopen ("log", "a");
      if (!d_peek_next_char (di)) 
        {
        fprintf(inslog, "  detected bug#C597B1, location#1");
        fclose(fp);
        }
      #endif
*/
