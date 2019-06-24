/*cp-demangle.c (d_expression_1): Fix out-of-bounds access. Check code variable for NULL before dereferencing it.*/
#define C597B1 = 1
/*cp-demangle.c (d_find_pack): Do not recurse for FIXED_TYPE, DEFAULT_ARG and NUMBER.*/
#define C597B2 = 1
/*cp-demangle.c (d_print_comp_inner): Add NULL pointer check.*/
#define C597B3 = 1
/*cp-demangle.h (d_peek_next_char): Define as inline function when CHECK_DEMANGLER is defined. (d_advance): Likewise.*/
#define C597B4 = 1
/*PR c++/67394 PR c++/70481 cplus-dem.c (squangle_mop_up): Zero bsize/ksize after freeing btypevec/ktypevec.*/
#define C2849B1 = 1
/*PR c++/70492 cplus-dem.c (gnu_special): Handle case where consume_count returns -1.*/
#define C2849B2 = 1
/*PR c++/70926 cplus-dem.c: Handle large values and overflow when demangling length variables.*/
#define C2849B3 = 1
/*cplus-dem.c (demangle_signature): After 'H', template function, no success and don't advance position if end of string reached. (demangle_template): After 'z', template name, return zero on premature end of string.*/
#define C3648B1 = 1

void print_detection(char id[], int i)
{
  FILE * inslog;
  inslog = fopen ("log", "a");
  fprintf(inslog, "  detected bug#%s, location#%d\n", id, i);
  close(inslog);
}
/*
#if C597B1 == 1
if (!d_peek_next_char (di)) 
  {
    FILE * inslog;
    inslog = fopen ("log", "a");
    fprintf(inslog, "  detected bug#C597B1, location#1");
    fclose(inslog);
  }
#endif
*/
