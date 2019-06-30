/*
 * No include guards here: this header is included by a public header to
 * get the `__BT_FUNC_STATUS_*` definitions locally, and then
 * <babeltrace2/undef-func-status.h> is included at the end of the
 * header to undefine all those definitions.
 *
 * If we forget to include <babeltrace2/undef-func-status.h> at the end
 * of a public header, we want to get the "redefined" compiler warning
 * to catch it.
 */

/*
 * Copyright (c) 2019 Philippe Proulx <pproulx@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * This is just extra protection, in case the user tries to include
 * <babeltrace2/func-status.h> in user code: this is a redminder that
 * this header is reserved for internal use.
 *
 * The correct way for a public header to include this is:
 *
 *     #define __BT_FUNC_STATUS_ENABLE
 *     #include <babeltrace2/func-status.h>
 *     #undef __BT_FUNC_STATUS_ENABLE
 */
#ifndef __BT_FUNC_STATUS_ENABLE
# error Do NOT include <babeltrace2/func-status.h> in user code.
#endif

/*
 * This is NOT part of the API.
 *
 * Do NOT use a `__BT_FUNC_STATUS_*` value in user code: this inhibits
 * precious type checking.
 */

/* Value is too large for the given data type */
#define __BT_FUNC_STATUS_OVERFLOW		-75

/* Invalid query parameters */
#define __BT_FUNC_STATUS_INVALID_PARAMS		-24

/* Invalid query object */
#define __BT_FUNC_STATUS_INVALID_OBJECT		-23

/* Memory allocation error */
#define __BT_FUNC_STATUS_MEMORY_ERROR		-12

/* Plugin loading error */
#define __BT_FUNC_STATUS_LOADING_ERROR		-2

/* General error */
#define __BT_FUNC_STATUS_ERROR			-1

/* Saul Goodman */
#define __BT_FUNC_STATUS_OK			0

/* End of iteration/consumption */
#define __BT_FUNC_STATUS_END			1

/* Something can't be found */
#define __BT_FUNC_STATUS_NOT_FOUND		2

/* Try operation again later */
#define __BT_FUNC_STATUS_AGAIN			11

/* Unsupported operation */
#define __BT_FUNC_STATUS_UNSUPPORTED		95

/* Object is canceled */
#define __BT_FUNC_STATUS_CANCELED		125
