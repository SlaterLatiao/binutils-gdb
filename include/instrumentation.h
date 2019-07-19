#ifndef INSTRUMENTATION_H
#define INSTRUMENTATION_H

/*cp-demangle.c (d_expression_1): Fix out-of-bounds access. Check code variable for NULL before dereferencing it.*/
#define C597B1 1
/*cp-demangle.c (d_find_pack): Do not recurse for FIXED_TYPE, DEFAULT_ARG and NUMBER.*/
#define C597B2 1
/*cp-demangle.c (d_print_comp_inner): Add NULL pointer check.*/
#define C597B3 1
/*cp-demangle.h (d_peek_next_char): Define as inline function when CHECK_DEMANGLER is defined. (d_advance): Likewise.*/
#define C597B4 1
/*PR c++/67394 PR c++/70481 cplus-dem.c (squangle_mop_up): Zero bsize/ksize after freeing btypevec/ktypevec.*/
#define C2849B1 1
/*PR c++/70492 cplus-dem.c (gnu_special): Handle case where consume_count returns -1.*/
#define C2849B2 1
/*PR c++/70926 cplus-dem.c: Handle large values and overflow when demangling length variables.*/
#define C2849B3 1
/*PR c++/71696 cplus-dem.c*/
#define C3339B1 1
/*cplus-dem.c (demangle_signature): After 'H', template function, no success and don't advance position if end of string reached. (demangle_template): After 'z', template name, return zero on premature end of string.*/
#define C3648B1 1
/*demangler crash with missing :? or fold expression component.*/
#define C3649B1 1
/*PR demangler/70909 PR demangler/67264 demangle.h (struct demangle_component): Add d_printing field. (cplus_demangle_print): Remove const qualifier from tree parameter. (cplus_demangle_print_callback): Likewise.*/
#define C4708B1 1
/*PR demangler/80513*/
#define C5675B1 1

#include <stdio.h>

static inline void
print_detection(const char *, int);

static inline void
print_detection(const char *id, int i)
{
  fprintf(stdout, "  %s - %d\n", id, i);
}


/*
#if C597B1 == 1
if (!d_peek_next_char (di))
  print_detection("C597B1", 1);
#endif
*/

#endif /* instrumentation.h */
