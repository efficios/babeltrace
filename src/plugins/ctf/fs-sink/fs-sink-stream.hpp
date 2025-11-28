/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_PLUGINS_CTF_FS_SINK_FS_SINK_STREAM_HPP
#define BABELTRACE_PLUGINS_CTF_FS_SINK_FS_SINK_STREAM_HPP

#include <glib.h>
#include <stdint.h>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2c/logging.hpp"
#include "ctfser/ctfser.h"

struct fs_sink_trace;
struct fs_sink_ctf_stream_class;

struct fs_sink_stream
{
    explicit fs_sink_stream(const bt2c::Logger& parentLogger) :
        logger {parentLogger, "PLUGIN/SINK.CTF.FS/STREAM"}
    {
    }

    bt2c::Logger logger;
    fs_sink_trace *trace = nullptr;
    bt_ctfser ctfser {};
    bt_ctfser ctfser_index {};

    /* Stream's file name */
    GString *file_name = nullptr;

    /* create and lttng index as well */
    bool lttng_index = false;

    /* Weak */
    const bt_stream *ir_stream = nullptr;

    fs_sink_ctf_stream_class *sc = nullptr;

    /* Current packet's state */
    struct
    {
        /*
         * True if we're, for this stream, within an opened
         * packet (got a packet beginning message, but no
         * packet end message yet).
         */
        bool is_open = false;

        /*
         * Current beginning default clock snapshot for the
         * current packet (`UINT64_C(-1)` if not set).
         */
        uint64_t beginning_cs = 0;

        /*
         * Current end default clock snapshot for the current
         * packet (`UINT64_C(-1)` if not set).
         */
        uint64_t end_cs = 0;

        /*
         * Current packet's content size (bits) for the current
         * packet.
         */
        uint64_t content_size = 0;

        /*
         * Current packet's total size (bits) for the current
         * packet.
         */
        uint64_t total_size = 0;

        /*
         * Discarded events (free running) counter for the
         * current packet.
         */
        uint64_t discarded_events_counter = 0;

        /* Sequence number (free running) of the current packet */
        uint64_t seq_num = 0;

        /*
         * Offset of the packet context structure within the
         * current packet (bits).
         */
        uint64_t context_offset_bits = 0;

        /*
         * Owned by this; `NULL` if the current packet is closed
         * or if the trace IR stream does not support packets.
         */
        const bt_packet *packet = nullptr;
    } packet_state;

    /* Previous packet's state */
    struct
    {
        /* End default clock snapshot (`UINT64_C(-1)` if not set) */
        uint64_t end_cs = 0;

        /* Discarded events (free running) counter */
        uint64_t discarded_events_counter = 0;

        /* Sequence number (free running) */
        uint64_t seq_num = 0;
    } prev_packet_state;

    /* State to handle discarded events */
    struct
    {
        /*
         * True if we're in the time range given by a previously
         * received discarded events message. In this case,
         * `beginning_cs` and `end_cs` below contain the
         * beginning and end clock snapshots for this range.
         *
         * This is used to validate that, when receiving a
         * packet end message, the current discarded events time
         * range matches what's expected for CTF 1.8, that is:
         *
         * * Its beginning time is the previous packet's end
         *   time (or the current packet's beginning time if
         *   this is the first packet).
         *
         * * Its end time is the current packet's end time.
         */
        bool in_range = false;

        /*
         * Beginning and end times of the time range given by a
         * previously received discarded events message.
         */
        uint64_t beginning_cs = 0;
        uint64_t end_cs = 0;
    } discarded_events_state;

    /* State to handle discarded packets */
    struct
    {
        /*
         * True if we're in the time range given by a previously
         * received discarded packets message. In this case,
         * `beginning_cs` and `end_cs` below contain the
         * beginning and end clock snapshots for this range.
         *
         * This is used to validate that, when receiving a
         * packet beginning message, the current discarded
         * packets time range matches what's expected for CTF
         * 1.8, that is:
         *
         * * Its beginning time is the previous packet's end
         *   time.
         *
         * * Its end time is the current packet's beginning
         *   time.
         */
        bool in_range = false;

        /*
         * Beginning and end times of the time range given by a
         * previously received discarded packets message.
         */
        uint64_t beginning_cs = 0;
        uint64_t end_cs = 0;
    } discarded_packets_state;

    struct
    {
        /*
         * Size of the index payload.
         */
        uint64_t content_size = 0;

        /*
         * Offset of the current packet in the file
         */
        uint64_t pkg_offset_in_file = 0;
    } lttng_index_state;
};

struct fs_sink_stream *fs_sink_stream_create(struct fs_sink_trace *trace,
                                             const bt_stream *ir_stream);

void fs_sink_stream_destroy(struct fs_sink_stream *stream);

int fs_sink_stream_write_event(struct fs_sink_stream *stream, const bt_clock_snapshot *cs,
                               const bt_event *event, struct fs_sink_ctf_event_class *ec);

int fs_sink_stream_open_packet(struct fs_sink_stream *stream, const bt_clock_snapshot *cs,
                               const bt_packet *packet);

int fs_sink_stream_close_packet(struct fs_sink_stream *stream, const bt_clock_snapshot *cs);

int fs_sink_stream_index_write_header(struct fs_sink_stream *stream);

int fs_sink_stream_index_write_packet_entry(struct fs_sink_stream *stream);

#endif /* BABELTRACE_PLUGINS_CTF_FS_SINK_FS_SINK_STREAM_HPP */
