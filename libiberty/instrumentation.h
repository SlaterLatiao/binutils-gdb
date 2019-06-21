/*(d_expression_1): Fix out-of-bounds access. Check code variable for NULL before dereferencing it.*/
#define C597B1 = 1
/*(d_find_pack): Do not recurse for FIXED_TYPE, DEFAULT_ARG and NUMBER.*/
#define C597B2 = 1

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
