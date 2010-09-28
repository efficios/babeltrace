#ifndef _BABELTRACE_COMPILER_H
#define _BABELTRACE_COMPILER_H

#define MAYBE_BUILD_BUG_ON(cond) ((void)sizeof(char[1 - 2 * !!(cond)]))

#endif /* _BABELTRACE_COMPILER_H */
