#include "cairoint.h"

#if CAIRO_HAS_HIDDEN_SYMBOLS
extern cairo_public int cairo_has_hidden_symbols;
int cairo_has_hidden_symbols;
#endif

int
main (void)
{
    return 0;
}
