#ifndef BABELTRACE_CTF_WRITER_CLOCK_H
#define BABELTRACE_CTF_WRITER_CLOCK_H

/*
 * BabelTrace - CTF Writer: Clock
 *
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#include <stdint.h>
#include <babeltrace/ref.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_ctf_clock;

/*
 * bt_ctf_clock_create: create a clock.
 *
 * Allocate a new clock setting its reference count to 1.
 *
 * @param name Name of the clock (will be copied); can be set to NULL
 *             for nameless clocks.
 *
 * Returns an allocated clock on success, NULL on error.
 */
extern struct bt_ctf_clock *bt_ctf_clock_create(const char *name);

/*
 * bt_ctf_clock_get_name: get a clock's name.
 *
 * Get the clock's name.
 *
 * @param clock Clock instance.
 *
 * Returns the clock's name, NULL on error.
 */
extern const char *bt_ctf_clock_get_name(struct bt_ctf_clock *clock);

/*
 * bt_ctf_clock_get_description: get a clock's description.
 *
 * Get the clock's description.
 *
 * @param clock Clock instance.
 *
 * Returns the clock's description, NULL if unset.
 */
extern const char *bt_ctf_clock_get_description(struct bt_ctf_clock *clock);

/*
 * bt_ctf_clock_set_description: set a clock's description.
 *
 * Set the clock's description. The description appears in the clock's TSDL
 * meta-data.
 *
 * @param clock Clock instance.
 * @param desc Description of the clock.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_clock_set_description(struct bt_ctf_clock *clock,
        const char *desc);

/*
 * bt_ctf_clock_get_frequency: get a clock's frequency.
 *
 * Get the clock's frequency (Hz).
 *
 * @param clock Clock instance.
 *
 * Returns the clock's frequency, -1ULL on error.
 */
extern uint64_t bt_ctf_clock_get_frequency(struct bt_ctf_clock *clock);

/*
 * bt_ctf_clock_set_frequency: set a clock's frequency.
 *
 * Set the clock's frequency (Hz).
 *
 * @param clock Clock instance.
 * @param freq Clock's frequency in Hz, defaults to 1 000 000 000 Hz (1ns).
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_clock_set_frequency(struct bt_ctf_clock *clock,
        uint64_t freq);

/*
 * bt_ctf_clock_get_precision: get a clock's precision.
 *
 * Get the clock's precision (in clock ticks).
 *
 * @param clock Clock instance.
 *
 * Returns the clock's precision, -1ULL on error.
 */
extern uint64_t bt_ctf_clock_get_precision(struct bt_ctf_clock *clock);

/*
 * bt_ctf_clock_set_precision: set a clock's precision.
 *
 * Set the clock's precision.
 *
 * @param clock Clock instance.
 * @param precision Clock's precision in clock ticks, defaults to 1.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_clock_set_precision(struct bt_ctf_clock *clock,
        uint64_t precision);

/*
 * bt_ctf_clock_get_offset_s: get a clock's offset in seconds.
 *
 * Get the clock's offset in seconds from POSIX.1 Epoch, 1970-01-01.
 *
 * @param clock Clock instance.
 * @param offset_s Pointer to clock offset in seconds (output).
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_clock_get_offset_s(struct bt_ctf_clock *clock,
        int64_t *offset_s);

/*
 * bt_ctf_clock_set_offset_s: set a clock's offset in seconds.
 *
 * Set the clock's offset in seconds from POSIX.1 Epoch, 1970-01-01,
 * defaults to 0.
 *
 * @param clock Clock instance.
 * @param offset_s Clock's offset in seconds, defaults to 0.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_clock_set_offset_s(struct bt_ctf_clock *clock,
        int64_t offset_s);

/*
 * bt_ctf_clock_get_offset: get a clock's offset in ticks.
 *
 * Get the clock's offset in ticks from Epoch + offset_t.
 *
 * @param clock Clock instance.
 * @param offset Clock offset in ticks from Epoch + offset_s (output).
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_clock_get_offset(struct bt_ctf_clock *clock,
        int64_t *offset);

/*
 * bt_ctf_clock_set_offset: set a clock's offset in ticks.
 *
 * Set the clock's offset in ticks from Epoch + offset_s.
 *
 * @param clock Clock instance.
 * @param offset Clock's offset in ticks from Epoch + offset_s, defaults to 0.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_clock_set_offset(struct bt_ctf_clock *clock,
        int64_t offset);

/*
 * bt_ctf_clock_get_is_absolute: get a clock's absolute attribute.
 *
 * Get the clock's absolute attribute. A clock is absolute if the clock is a
 * global reference across the trace's other clocks.
 *
 * @param clock Clock instance.
 *
 * Returns the clock's absolute attribute, a negative value on error.
 */
extern int bt_ctf_clock_get_is_absolute(struct bt_ctf_clock *clock);

/*
 * bt_ctf_clock_set_is_absolute: set a clock's absolute attribute.
 *
 * Set the clock's absolute attribute. A clock is absolute if the clock is a
 * global reference across the trace's other clocks.
 *
 * @param clock Clock instance.
 * @param is_absolute Clock's absolute attribute, defaults to FALSE.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_clock_set_is_absolute(struct bt_ctf_clock *clock,
        int is_absolute);

/*
 * bt_ctf_clock_get_uuid: get a clock's UUID.
 *
 * Get the clock's UUID.
 *
 * @param clock Clock instance.
 *
 * Returns a pointer to the clock's UUID (16 byte array) on success,
 * NULL on error.
 */
extern const unsigned char *bt_ctf_clock_get_uuid(struct bt_ctf_clock *clock);

/*
 * bt_ctf_clock_set_uuid: set a clock's UUID.
 *
 * Set a clock's UUID.
 *
 * @param clock Clock instance.
 * @param uuid A 16-byte array containing a UUID.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_clock_set_uuid(struct bt_ctf_clock *clock,
        const unsigned char *uuid);

/*
 * bt_ctf_clock_set_time: set a clock's current time value.
 *
 * Set the current time in nanoseconds since the clock's origin (offset and
 * offset_s attributes). Defaults to 0.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_clock_set_time(struct bt_ctf_clock *clock,
		int64_t time);

/*
 * bt_ctf_clock_get and bt_ctf_clock_put: increment and decrement the
 * refcount of the clock
 *
 * You may also use bt_ctf_get() and bt_ctf_put() with clock objects.
 *
 * These functions ensure that the clock won't be destroyed when it
 * is in use. The same number of get and put (plus one extra put to
 * release the initial reference done at creation) has to be done to
 * destroy a clock.
 *
 * When the clock refcount is decremented to 0 by a bt_ctf_clock_put,
 * the clock is freed.
 *
 * @param clock Clock instance.
 */

/* Pre-2.0 CTF writer compatibility */
static inline
void bt_ctf_clock_get(struct bt_ctf_clock *clock)
{
        bt_get(clock);
}

/* Pre-2.0 CTF writer compatibility */
static inline
void bt_ctf_clock_put(struct bt_ctf_clock *clock)
{
        bt_put(clock);
}

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_WRITER_CLOCK_H */
