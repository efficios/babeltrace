/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2010-2019 EfficiOS Inc. and Linux Foundation
 */

/*
 * No include guards here: it is safe to include this file multiple
 * times.
 */

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

/*
 * This is NOT part of the API.
 *
 * Do NOT use a `__BT_FUNC_STATUS_*` value in user code: this inhibits
 * precious type checking.
 */

/* Value is too large for the given data type */
#ifndef __BT_FUNC_STATUS_OVERFLOW_ERROR
# define __BT_FUNC_STATUS_OVERFLOW_ERROR	-75
#endif

/* Memory allocation error */
#ifndef __BT_FUNC_STATUS_MEMORY_ERROR
# define __BT_FUNC_STATUS_MEMORY_ERROR		-12
#endif

/* User function error */
#ifndef __BT_FUNC_STATUS_USER_ERROR
# define __BT_FUNC_STATUS_USER_ERROR		-2
#endif

/* General error */
#ifndef __BT_FUNC_STATUS_ERROR
# define __BT_FUNC_STATUS_ERROR			-1
#endif

/* Saul Goodman */
#ifndef __BT_FUNC_STATUS_OK
# define __BT_FUNC_STATUS_OK			0
#endif

/* End of iteration/consumption */
#ifndef __BT_FUNC_STATUS_END
# define __BT_FUNC_STATUS_END			1
#endif

/* Something can't be found */
#ifndef __BT_FUNC_STATUS_NOT_FOUND
# define __BT_FUNC_STATUS_NOT_FOUND		2
#endif

/* Object is interrupted */
#ifndef __BT_FUNC_STATUS_INTERRUPTED
# define __BT_FUNC_STATUS_INTERRUPTED		4
#endif

/* No match found */
#ifndef __BT_FUNC_STATUS_NO_MATCH
# define __BT_FUNC_STATUS_NO_MATCH		6
#endif

/* Try operation again later */
#ifndef __BT_FUNC_STATUS_AGAIN
# define __BT_FUNC_STATUS_AGAIN			11
#endif

/* Unknown query object */
#ifndef __BT_FUNC_STATUS_UNKNOWN_OBJECT
# define __BT_FUNC_STATUS_UNKNOWN_OBJECT	42
#endif
