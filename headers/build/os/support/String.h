/*
 * Build-system shadow for <string.h>.
 * On a case-sensitive filesystem, #include_next correctly reaches the real
 * system string.h. The C++ BString class is guarded so C TUs are unaffected.
 */
#include_next <string.h>

#ifdef __cplusplus
#  ifndef _BUILD_BSTRING_SHADOW_INCLUDED
#  define _BUILD_BSTRING_SHADOW_INCLUDED
#    include <../os/support/String.h>
#  endif
#endif
