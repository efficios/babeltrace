/*
 * input.c
 *
 * Babeltrace Component Input
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/component/input.h>
#include <babeltrace/ref.h>

BT_HIDDEN
int component_input_init(struct component_input *input)
{
	input->min_count = 1;
	input->max_count = 1;
	input->iterators = g_ptr_array_new_with_free_func(bt_put);
	if (!input->iterators) {
		return 1;
	}
	return 0;
}

BT_HIDDEN
int component_input_validate(struct component_input *input)
{
	if (input->min_count > input->max_count) {
		printf_error("Invalid component configuration; minimum input count > maximum input count.");
		return 1;
	}
	return 0;
}

BT_HIDDEN
void component_input_fini(struct component_input *input)
{
	g_ptr_array_free(input->iterators, TRUE);
}
