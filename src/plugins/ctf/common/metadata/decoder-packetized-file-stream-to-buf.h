/*
 * Copyright 2019 - Efficios, Inc.
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
 */

#ifndef SRC_PLUGINS_CTF_COMMON_METADATA_DECODER_PACKETIZED_FILE_STREAM_TO_BUF
#define SRC_PLUGINS_CTF_COMMON_METADATA_DECODER_PACKETIZED_FILE_STREAM_TO_BUF

#include <stdbool.h>
#include <stdint.h>

#include <babeltrace2/babeltrace.h>

BT_HIDDEN
int ctf_metadata_decoder_packetized_file_stream_to_buf(FILE *fp,
		char **buf, int byte_order, bool *is_uuid_set,
		uint8_t *uuid, bt_logging_level log_level,
		bt_self_component *self_comp);

#endif /* SRC_PLUGINS_CTF_COMMON_METADATA_DECODER_PACKETIZED_FILE_STREAM_TO_BUF */
