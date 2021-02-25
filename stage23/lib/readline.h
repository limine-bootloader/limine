#ifndef __LIB__READLINE_H__
#define __LIB__READLINE_H__

#include <stddef.h>

#define GETCHAR_CURSOR_LEFT  (-10)
#define GETCHAR_CURSOR_RIGHT (-11)
#define GETCHAR_CURSOR_UP    (-12)
#define GETCHAR_CURSOR_DOWN  (-13)
#define GETCHAR_DELETE       (-14)
#define GETCHAR_END          (-15)
#define GETCHAR_HOME         (-16)
#define GETCHAR_PGUP         (-17)
#define GETCHAR_PGDOWN       (-18)
#define GETCHAR_F10          (-19)

int getchar(void);
void readline(const char *orig_str, char *buf, size_t limit);

#endif
