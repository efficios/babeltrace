/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 Efficios Inc.
 */

#ifndef SRC_PLUGINS_CTF_COMMON_METADATA_DECODER_PACKETIZED_FILE_STREAM_TO_BUF
#define SRC_PLUGINS_CTF_COMMON_METADATA_DECODER_PACKETIZED_FILE_STREAM_TO_BUF

#include <cstdio>

#include <stdint.h>

#include <babeltrace2/babeltrace.h>

int ctf_metadata_decoder_packetized_file_stream_to_buf(FILE *fp, char **buf, int byte_order,
                                                       bool *is_uuid_set, uint8_t *uuid,
                                                       bt_logging_level log_level,
                                                       bt_self_component *self_comp,
                                                       bt_self_component_class *self_comp_class);

#endif /* SRC_PLUGINS_CTF_COMMON_METADATA_DECODER_PACKETIZED_FILE_STREAM_TO_BUF */
