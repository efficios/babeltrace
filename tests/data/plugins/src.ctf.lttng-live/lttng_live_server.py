# The MIT License (MIT)
#
# Copyright (c) 2019 Philippe Proulx <pproulx@efficios.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

import argparse
import collections.abc
import logging
import os
import os.path
import re
import socket
import struct
import sys
import tempfile


class UnexpectedInput(RuntimeError):
    pass


class _LttngLiveViewerCommand:
    def __init__(self, version):
        self._version = version

    @property
    def version(self):
        return self._version


class _LttngLiveViewerConnectCommand(_LttngLiveViewerCommand):
    def __init__(self, version, viewer_session_id, major, minor):
        super().__init__(version)
        self._viewer_session_id = viewer_session_id
        self._major = major
        self._minor = minor

    @property
    def viewer_session_id(self):
        return self._viewer_session_id

    @property
    def major(self):
        return self._major

    @property
    def minor(self):
        return self._minor


class _LttngLiveViewerConnectReply:
    def __init__(self, viewer_session_id, major, minor):
        self._viewer_session_id = viewer_session_id
        self._major = major
        self._minor = minor

    @property
    def viewer_session_id(self):
        return self._viewer_session_id

    @property
    def major(self):
        return self._major

    @property
    def minor(self):
        return self._minor


class _LttngLiveViewerGetTracingSessionInfosCommand(_LttngLiveViewerCommand):
    pass


class _LttngLiveViewerTracingSessionInfo:
    def __init__(
        self,
        tracing_session_id,
        live_timer_freq,
        client_count,
        stream_count,
        hostname,
        name,
    ):
        self._tracing_session_id = tracing_session_id
        self._live_timer_freq = live_timer_freq
        self._client_count = client_count
        self._stream_count = stream_count
        self._hostname = hostname
        self._name = name

    @property
    def tracing_session_id(self):
        return self._tracing_session_id

    @property
    def live_timer_freq(self):
        return self._live_timer_freq

    @property
    def client_count(self):
        return self._client_count

    @property
    def stream_count(self):
        return self._stream_count

    @property
    def hostname(self):
        return self._hostname

    @property
    def name(self):
        return self._name


class _LttngLiveViewerGetTracingSessionInfosReply:
    def __init__(self, tracing_session_infos):
        self._tracing_session_infos = tracing_session_infos

    @property
    def tracing_session_infos(self):
        return self._tracing_session_infos


class _LttngLiveViewerAttachToTracingSessionCommand(_LttngLiveViewerCommand):
    class SeekType:
        BEGINNING = 1
        LAST = 2

    def __init__(self, version, tracing_session_id, offset, seek_type):
        super().__init__(version)
        self._tracing_session_id = tracing_session_id
        self._offset = offset
        self._seek_type = seek_type

    @property
    def tracing_session_id(self):
        return self._tracing_session_id

    @property
    def offset(self):
        return self._offset

    @property
    def seek_type(self):
        return self._seek_type


class _LttngLiveViewerStreamInfo:
    def __init__(self, id, trace_id, is_metadata, path, channel_name):
        self._id = id
        self._trace_id = trace_id
        self._is_metadata = is_metadata
        self._path = path
        self._channel_name = channel_name

    @property
    def id(self):
        return self._id

    @property
    def trace_id(self):
        return self._trace_id

    @property
    def is_metadata(self):
        return self._is_metadata

    @property
    def path(self):
        return self._path

    @property
    def channel_name(self):
        return self._channel_name


class _LttngLiveViewerAttachToTracingSessionReply:
    class Status:
        OK = 1
        ALREADY = 2
        UNKNOWN = 3
        NOT_LIVE = 4
        SEEK_ERROR = 5
        NO_SESSION = 6

    def __init__(self, status, stream_infos):
        self._status = status
        self._stream_infos = stream_infos

    @property
    def status(self):
        return self._status

    @property
    def stream_infos(self):
        return self._stream_infos


class _LttngLiveViewerGetNextDataStreamIndexEntryCommand(_LttngLiveViewerCommand):
    def __init__(self, version, stream_id):
        super().__init__(version)
        self._stream_id = stream_id

    @property
    def stream_id(self):
        return self._stream_id


class _LttngLiveViewerGetNextDataStreamIndexEntryReply:
    class Status:
        OK = 1
        RETRY = 2
        HUP = 3
        ERROR = 4
        INACTIVE = 5
        EOF = 6

    def __init__(self, status, index_entry, has_new_metadata, has_new_data_stream):
        self._status = status
        self._index_entry = index_entry
        self._has_new_metadata = has_new_metadata
        self._has_new_data_stream = has_new_data_stream

    @property
    def status(self):
        return self._status

    @property
    def index_entry(self):
        return self._index_entry

    @property
    def has_new_metadata(self):
        return self._has_new_metadata

    @property
    def has_new_data_stream(self):
        return self._has_new_data_stream


class _LttngLiveViewerGetDataStreamPacketDataCommand(_LttngLiveViewerCommand):
    def __init__(self, version, stream_id, offset, req_length):
        super().__init__(version)
        self._stream_id = stream_id
        self._offset = offset
        self._req_length = req_length

    @property
    def stream_id(self):
        return self._stream_id

    @property
    def offset(self):
        return self._offset

    @property
    def req_length(self):
        return self._req_length


class _LttngLiveViewerGetDataStreamPacketDataReply:
    class Status:
        OK = 1
        RETRY = 2
        ERROR = 3
        EOF = 4

    def __init__(self, status, data, has_new_metadata, has_new_data_stream):
        self._status = status
        self._data = data
        self._has_new_metadata = has_new_metadata
        self._has_new_data_stream = has_new_data_stream

    @property
    def status(self):
        return self._status

    @property
    def data(self):
        return self._data

    @property
    def has_new_metadata(self):
        return self._has_new_metadata

    @property
    def has_new_data_stream(self):
        return self._has_new_data_stream


class _LttngLiveViewerGetMetadataStreamDataCommand(_LttngLiveViewerCommand):
    def __init__(self, version, stream_id):
        super().__init__(version)
        self._stream_id = stream_id

    @property
    def stream_id(self):
        return self._stream_id


class _LttngLiveViewerGetMetadataStreamDataContentReply:
    class Status:
        OK = 1
        NO_NEW = 2
        ERROR = 3

    def __init__(self, status, data):
        self._status = status
        self._data = data

    @property
    def status(self):
        return self._status

    @property
    def data(self):
        return self._data


class _LttngLiveViewerGetNewStreamInfosCommand(_LttngLiveViewerCommand):
    def __init__(self, version, tracing_session_id):
        super().__init__(version)
        self._tracing_session_id = tracing_session_id

    @property
    def tracing_session_id(self):
        return self._tracing_session_id


class _LttngLiveViewerGetNewStreamInfosReply:
    class Status:
        OK = 1
        NO_NEW = 2
        ERROR = 3
        HUP = 4

    def __init__(self, status, stream_infos):
        self._status = status
        self._stream_infos = stream_infos

    @property
    def status(self):
        return self._status

    @property
    def stream_infos(self):
        return self._stream_infos


class _LttngLiveViewerCreateViewerSessionCommand(_LttngLiveViewerCommand):
    pass


class _LttngLiveViewerCreateViewerSessionReply:
    class Status:
        OK = 1
        ERROR = 2

    def __init__(self, status):
        self._status = status

    @property
    def status(self):
        return self._status


class _LttngLiveViewerDetachFromTracingSessionCommand(_LttngLiveViewerCommand):
    def __init__(self, version, tracing_session_id):
        super().__init__(version)
        self._tracing_session_id = tracing_session_id

    @property
    def tracing_session_id(self):
        return self._tracing_session_id


class _LttngLiveViewerDetachFromTracingSessionReply:
    class Status:
        OK = 1
        UNKNOWN = 2
        ERROR = 3

    def __init__(self, status):
        self._status = status

    @property
    def status(self):
        return self._status


# An LTTng live protocol codec can convert bytes to command objects and
# reply objects to bytes.
class _LttngLiveViewerProtocolCodec:
    _COMMAND_HEADER_STRUCT_FMT = 'QII'
    _COMMAND_HEADER_SIZE_BYTES = struct.calcsize(_COMMAND_HEADER_STRUCT_FMT)

    def __init__(self):
        pass

    def _unpack(self, fmt, data, offset=0):
        fmt = '!' + fmt
        return struct.unpack_from(fmt, data, offset)

    def _unpack_payload(self, fmt, data):
        return self._unpack(
            fmt, data, _LttngLiveViewerProtocolCodec._COMMAND_HEADER_SIZE_BYTES
        )

    def decode(self, data):
        if len(data) < self._COMMAND_HEADER_SIZE_BYTES:
            # Not enough data to read the command header
            return

        payload_size, cmd_type, version = self._unpack(
            self._COMMAND_HEADER_STRUCT_FMT, data
        )
        logging.info(
            'Decoded command header: payload-size={}, cmd-type={}, version={}'.format(
                payload_size, cmd_type, version
            )
        )

        if len(data) < self._COMMAND_HEADER_SIZE_BYTES + payload_size:
            # Not enough data to read the whole command
            return

        if cmd_type == 1:
            viewer_session_id, major, minor, conn_type = self._unpack_payload(
                'QIII', data
            )
            return _LttngLiveViewerConnectCommand(
                version, viewer_session_id, major, minor
            )
        elif cmd_type == 2:
            return _LttngLiveViewerGetTracingSessionInfosCommand(version)
        elif cmd_type == 3:
            tracing_session_id, offset, seek_type = self._unpack_payload('QQI', data)
            return _LttngLiveViewerAttachToTracingSessionCommand(
                version, tracing_session_id, offset, seek_type
            )
        elif cmd_type == 4:
            (stream_id,) = self._unpack_payload('Q', data)
            return _LttngLiveViewerGetNextDataStreamIndexEntryCommand(
                version, stream_id
            )
        elif cmd_type == 5:
            stream_id, offset, req_length = self._unpack_payload('QQI', data)
            return _LttngLiveViewerGetDataStreamPacketDataCommand(
                version, stream_id, offset, req_length
            )
        elif cmd_type == 6:
            (stream_id,) = self._unpack_payload('Q', data)
            return _LttngLiveViewerGetMetadataStreamDataCommand(version, stream_id)
        elif cmd_type == 7:
            (tracing_session_id,) = self._unpack_payload('Q', data)
            return _LttngLiveViewerGetNewStreamInfosCommand(version, tracing_session_id)
        elif cmd_type == 8:
            return _LttngLiveViewerCreateViewerSessionCommand(version)
        elif cmd_type == 9:
            (tracing_session_id,) = self._unpack_payload('Q', data)
            return _LttngLiveViewerDetachFromTracingSessionCommand(
                version, tracing_session_id
            )
        else:
            raise UnexpectedInput('Unknown command type {}'.format(cmd_type))

    def _pack(self, fmt, *args):
        # Force network byte order
        return struct.pack('!' + fmt, *args)

    def _encode_zero_padded_str(self, string, length):
        data = string.encode()
        return data.ljust(length, b'\x00')

    def _encode_stream_info(self, info):
        data = self._pack('QQI', info.id, info.trace_id, int(info.is_metadata))
        data += self._encode_zero_padded_str(info.path, 4096)
        data += self._encode_zero_padded_str(info.channel_name, 255)
        return data

    def _get_has_new_stuff_flags(self, has_new_metadata, has_new_data_streams):
        flags = 0

        if has_new_metadata:
            flags |= 1

        if has_new_data_streams:
            flags |= 2

        return flags

    def encode(self, reply):
        if type(reply) is _LttngLiveViewerConnectReply:
            data = self._pack(
                'QIII', reply.viewer_session_id, reply.major, reply.minor, 2
            )
        elif type(reply) is _LttngLiveViewerGetTracingSessionInfosReply:
            data = self._pack('I', len(reply.tracing_session_infos))

            for info in reply.tracing_session_infos:
                data += self._pack(
                    'QIII',
                    info.tracing_session_id,
                    info.live_timer_freq,
                    info.client_count,
                    info.stream_count,
                )
                data += self._encode_zero_padded_str(info.hostname, 64)
                data += self._encode_zero_padded_str(info.name, 255)
        elif type(reply) is _LttngLiveViewerAttachToTracingSessionReply:
            data = self._pack('II', reply.status, len(reply.stream_infos))

            for info in reply.stream_infos:
                data += self._encode_stream_info(info)
        elif type(reply) is _LttngLiveViewerGetNextDataStreamIndexEntryReply:
            entry = reply.index_entry
            flags = self._get_has_new_stuff_flags(
                reply.has_new_metadata, reply.has_new_data_stream
            )

            data = self._pack(
                'QQQQQQQII',
                entry.offset_bytes,
                entry.total_size_bits,
                entry.content_size_bits,
                entry.timestamp_begin,
                entry.timestamp_end,
                entry.events_discarded,
                entry.stream_class_id,
                reply.status,
                flags,
            )
        elif type(reply) is _LttngLiveViewerGetDataStreamPacketDataReply:
            flags = self._get_has_new_stuff_flags(
                reply.has_new_metadata, reply.has_new_data_stream
            )
            data = self._pack('III', reply.status, len(reply.data), flags)
            data += reply.data
        elif type(reply) is _LttngLiveViewerGetMetadataStreamDataContentReply:
            data = self._pack('QI', len(reply.data), reply.status)
            data += reply.data
        elif type(reply) is _LttngLiveViewerGetNewStreamInfosReply:
            data = self._pack('II', reply.status, len(reply.stream_infos))

            for info in reply.stream_infos:
                data += self._encode_stream_info(info)
        elif type(reply) is _LttngLiveViewerCreateViewerSessionReply:
            data = self._pack('I', reply.status)
        elif type(reply) is _LttngLiveViewerDetachFromTracingSessionReply:
            data = self._pack('I', reply.status)
        else:
            raise ValueError(
                'Unknown reply object with class `{}`'.format(reply.__class__.__name__)
            )

        return data


# An entry within the index of an LTTng data stream.
class _LttngDataStreamIndexEntry:
    def __init__(
        self,
        offset_bytes,
        total_size_bits,
        content_size_bits,
        timestamp_begin,
        timestamp_end,
        events_discarded,
        stream_class_id,
    ):
        self._offset_bytes = offset_bytes
        self._total_size_bits = total_size_bits
        self._content_size_bits = content_size_bits
        self._timestamp_begin = timestamp_begin
        self._timestamp_end = timestamp_end
        self._events_discarded = events_discarded
        self._stream_class_id = stream_class_id

    @property
    def offset_bytes(self):
        return self._offset_bytes

    @property
    def total_size_bits(self):
        return self._total_size_bits

    @property
    def total_size_bytes(self):
        return self._total_size_bits // 8

    @property
    def content_size_bits(self):
        return self._content_size_bits

    @property
    def content_size_bytes(self):
        return self._content_size_bits // 8

    @property
    def timestamp_begin(self):
        return self._timestamp_begin

    @property
    def timestamp_end(self):
        return self._timestamp_end

    @property
    def events_discarded(self):
        return self._events_discarded

    @property
    def stream_class_id(self):
        return self._stream_class_id


# The index of an LTTng data stream, a sequence of index entries.
class _LttngDataStreamIndex(collections.abc.Sequence):
    def __init__(self, path):
        self._path = path
        self._build()
        logging.info(
            'Built data stream index entries: path="{}", count={}'.format(
                path, len(self._entries)
            )
        )

    def _build(self):
        self._entries = []
        assert os.path.isfile(self._path)

        with open(self._path, 'rb') as f:
            # Read header first
            fmt = '>IIII'
            size = struct.calcsize(fmt)
            data = f.read(size)
            assert len(data) == size
            magic, index_major, index_minor, index_entry_length = struct.unpack(
                fmt, data
            )
            assert magic == 0xC1F1DCC1

            # Read index entries
            fmt = '>QQQQQQQ'
            size = struct.calcsize(fmt)

            while True:
                logging.debug(
                    'Decoding data stream index entry: path="{}", offset={}'.format(
                        self._path, f.tell()
                    )
                )
                data = f.read(size)

                if not data:
                    # Done
                    break

                assert len(data) == size
                (
                    offset_bytes,
                    total_size_bits,
                    content_size_bits,
                    timestamp_begin,
                    timestamp_end,
                    events_discarded,
                    stream_class_id,
                ) = struct.unpack(fmt, data)

                self._entries.append(
                    _LttngDataStreamIndexEntry(
                        offset_bytes,
                        total_size_bits,
                        content_size_bits,
                        timestamp_begin,
                        timestamp_end,
                        events_discarded,
                        stream_class_id,
                    )
                )

                # Skip anything else before the next entry
                f.seek(index_entry_length - size, os.SEEK_CUR)

    def __getitem__(self, index):
        return self._entries[index]

    def __len__(self):
        return len(self._entries)

    @property
    def path(self):
        return self._path


# An LTTng data stream.
class _LttngDataStream:
    def __init__(self, path):
        self._path = path
        filename = os.path.basename(path)
        match = re.match(r'(.*)_\d+', filename)
        self._channel_name = match.group(1)
        trace_dir = os.path.dirname(path)
        index_path = os.path.join(trace_dir, 'index', filename + '.idx')
        self._index = _LttngDataStreamIndex(index_path)
        assert os.path.isfile(path)
        self._file = open(path, 'rb')
        logging.info(
            'Built data stream: path="{}", channel-name="{}"'.format(
                path, self._channel_name
            )
        )

    @property
    def path(self):
        return self._path

    @property
    def channel_name(self):
        return self._channel_name

    @property
    def index(self):
        return self._index

    def get_data(self, offset_bytes, len_bytes):
        self._file.seek(offset_bytes)
        return self._file.read(len_bytes)


# An LTTng metadata stream.
class _LttngMetadataStream:
    def __init__(self, path):
        self._path = path
        logging.info('Built metadata stream: path="{}"'.format(path))

    @property
    def path(self):
        return self._path

    @property
    def data(self):
        assert os.path.isfile(self._path)

        with open(self._path, 'rb') as f:
            return f.read()


# An LTTng trace, a sequence of LTTng data streams.
class LttngTrace(collections.abc.Sequence):
    def __init__(self, trace_dir):
        assert os.path.isdir(trace_dir)
        self._path = trace_dir
        self._metadata_stream = _LttngMetadataStream(
            os.path.join(trace_dir, 'metadata')
        )
        self._create_data_streams(trace_dir)
        logging.info('Built trace: path="{}"'.format(trace_dir))

    def _create_data_streams(self, trace_dir):
        data_stream_paths = []

        for filename in os.listdir(trace_dir):
            path = os.path.join(trace_dir, filename)

            if not os.path.isfile(path):
                continue

            if filename.startswith('.'):
                continue

            if filename == 'metadata':
                continue

            data_stream_paths.append(path)

        data_stream_paths.sort()
        self._data_streams = []

        for data_stream_path in data_stream_paths:
            self._data_streams.append(_LttngDataStream(data_stream_path))

    @property
    def path(self):
        return self._path

    @property
    def metadata_stream(self):
        return self._metadata_stream

    def __getitem__(self, index):
        return self._data_streams[index]

    def __len__(self):
        return len(self._data_streams)


# The state of a single data stream.
class _LttngLiveViewerSessionDataStreamState:
    def __init__(self, ts_state, info, data_stream):
        self._ts_state = ts_state
        self._info = info
        self._data_stream = data_stream
        self._cur_index_entry_index = 0
        fmt = 'Built data stream state: id={}, ts-id={}, ts-name="{}", path="{}"'
        logging.info(
            fmt.format(
                info.id,
                ts_state.tracing_session_descriptor.info.tracing_session_id,
                ts_state.tracing_session_descriptor.info.name,
                data_stream.path,
            )
        )

    @property
    def tracing_session_state(self):
        return self._ts_state

    @property
    def info(self):
        return self._info

    @property
    def data_stream(self):
        return self._data_stream

    @property
    def cur_index_entry(self):
        if self._cur_index_entry_index == len(self._data_stream.index):
            return

        return self._data_stream.index[self._cur_index_entry_index]

    def goto_next_index_entry(self):
        self._cur_index_entry_index += 1


# The state of a single metadata stream.
class _LttngLiveViewerSessionMetadataStreamState:
    def __init__(self, ts_state, info, metadata_stream):
        self._ts_state = ts_state
        self._info = info
        self._metadata_stream = metadata_stream
        self._is_sent = False
        fmt = 'Built metadata stream state: id={}, ts-id={}, ts-name="{}", path="{}"'
        logging.info(
            fmt.format(
                info.id,
                ts_state.tracing_session_descriptor.info.tracing_session_id,
                ts_state.tracing_session_descriptor.info.name,
                metadata_stream.path,
            )
        )

    @property
    def trace_session_state(self):
        return self._trace_session_state

    @property
    def info(self):
        return self._info

    @property
    def metadata_stream(self):
        return self._metadata_stream

    @property
    def is_sent(self):
        return self._is_sent

    @is_sent.setter
    def is_sent(self, value):
        self._is_sent = value


# The state of a tracing session.
class _LttngLiveViewerSessionTracingSessionState:
    def __init__(self, tc_descr, base_stream_id):
        self._tc_descr = tc_descr
        self._stream_infos = []
        self._ds_states = {}
        self._ms_states = {}
        stream_id = base_stream_id

        for trace in tc_descr.traces:
            trace_id = stream_id * 1000

            # Data streams -> stream infos and data stream states
            for data_stream in trace:
                info = _LttngLiveViewerStreamInfo(
                    stream_id,
                    trace_id,
                    False,
                    data_stream.path,
                    data_stream.channel_name,
                )
                self._stream_infos.append(info)
                self._ds_states[stream_id] = _LttngLiveViewerSessionDataStreamState(
                    self, info, data_stream
                )
                stream_id += 1

            # Metadata stream -> stream info and metadata stream state
            info = _LttngLiveViewerStreamInfo(
                stream_id, trace_id, True, trace.metadata_stream.path, 'metadata'
            )
            self._stream_infos.append(info)
            self._ms_states[stream_id] = _LttngLiveViewerSessionMetadataStreamState(
                self, info, trace.metadata_stream
            )
            stream_id += 1

        self._is_attached = False
        fmt = 'Built tracing session state: id={}, name="{}"'
        logging.info(fmt.format(tc_descr.info.tracing_session_id, tc_descr.info.name))

    @property
    def tracing_session_descriptor(self):
        return self._tc_descr

    @property
    def data_stream_states(self):
        return self._ds_states

    @property
    def metadata_stream_states(self):
        return self._ms_states

    @property
    def stream_infos(self):
        return self._stream_infos

    @property
    def has_new_metadata(self):
        return any([not ms.is_sent for ms in self._ms_states.values()])

    @property
    def is_attached(self):
        return self._is_attached

    @is_attached.setter
    def is_attached(self, value):
        self._is_attached = value


# An LTTng live viewer session manages a view on tracing sessions
# and replies to commands accordingly.
class _LttngLiveViewerSession:
    def __init__(
        self,
        viewer_session_id,
        tracing_session_descriptors,
        max_query_data_response_size,
    ):
        self._viewer_session_id = viewer_session_id
        self._ts_states = {}
        self._stream_states = {}
        self._max_query_data_response_size = max_query_data_response_size
        total_stream_infos = 0

        for ts_descr in tracing_session_descriptors:
            ts_state = _LttngLiveViewerSessionTracingSessionState(
                ts_descr, total_stream_infos
            )
            ts_id = ts_state.tracing_session_descriptor.info.tracing_session_id
            self._ts_states[ts_id] = ts_state
            total_stream_infos += len(ts_state.stream_infos)

            # Update session's stream states to have the new states
            self._stream_states.update(ts_state.data_stream_states)
            self._stream_states.update(ts_state.metadata_stream_states)

        self._command_handlers = {
            _LttngLiveViewerAttachToTracingSessionCommand: self._handle_attach_to_tracing_session_command,
            _LttngLiveViewerCreateViewerSessionCommand: self._handle_create_viewer_session_command,
            _LttngLiveViewerDetachFromTracingSessionCommand: self._handle_detach_from_tracing_session_command,
            _LttngLiveViewerGetDataStreamPacketDataCommand: self._handle_get_data_stream_packet_data_command,
            _LttngLiveViewerGetMetadataStreamDataCommand: self._handle_get_metadata_stream_data_command,
            _LttngLiveViewerGetNewStreamInfosCommand: self._handle_get_new_stream_infos_command,
            _LttngLiveViewerGetNextDataStreamIndexEntryCommand: self._handle_get_next_data_stream_index_entry_command,
            _LttngLiveViewerGetTracingSessionInfosCommand: self._handle_get_tracing_session_infos_command,
        }

    @property
    def viewer_session_id(self):
        return self._viewer_session_id

    def _get_tracing_session_state(self, tracing_session_id):
        if tracing_session_id not in self._ts_states:
            raise UnexpectedInput(
                'Unknown tracing session ID {}'.format(tracing_session_id)
            )

        return self._ts_states[tracing_session_id]

    def _get_stream_state(self, stream_id):
        if stream_id not in self._stream_states:
            UnexpectedInput('Unknown stream ID {}'.format(stream_id))

        return self._stream_states[stream_id]

    def handle_command(self, cmd):
        logging.info(
            'Handling command in viewer session: cmd-cls-name={}'.format(
                cmd.__class__.__name__
            )
        )
        cmd_type = type(cmd)

        if cmd_type not in self._command_handlers:
            raise UnexpectedInput(
                'Unexpected command: cmd-cls-name={}'.format(cmd.__class__.__name__)
            )

        return self._command_handlers[cmd_type](cmd)

    def _handle_attach_to_tracing_session_command(self, cmd):
        fmt = 'Handling "attach to tracing session" command: ts-id={}, offset={}, seek-type={}'
        logging.info(fmt.format(cmd.tracing_session_id, cmd.offset, cmd.seek_type))
        ts_state = self._get_tracing_session_state(cmd.tracing_session_id)
        info = ts_state.tracing_session_descriptor.info

        if ts_state.is_attached:
            raise UnexpectedInput(
                'Cannot attach to tracing session `{}`: viewer is already attached'.format(
                    info.name
                )
            )

        ts_state.is_attached = True
        status = _LttngLiveViewerAttachToTracingSessionReply.Status.OK
        return _LttngLiveViewerAttachToTracingSessionReply(
            status, ts_state.stream_infos
        )

    def _handle_detach_from_tracing_session_command(self, cmd):
        fmt = 'Handling "detach from tracing session" command: ts-id={}'
        logging.info(fmt.format(cmd.tracing_session_id))
        ts_state = self._get_tracing_session_state(cmd.tracing_session_id)
        info = ts_state.tracing_session_descriptor.info

        if not ts_state.is_attached:
            raise UnexpectedInput(
                'Cannot detach to tracing session `{}`: viewer is not attached'.format(
                    info.name
                )
            )

        ts_state.is_attached = False
        status = _LttngLiveViewerDetachFromTracingSessionReply.Status.OK
        return _LttngLiveViewerDetachFromTracingSessionReply(status)

    def _handle_get_next_data_stream_index_entry_command(self, cmd):
        fmt = 'Handling "get next data stream index entry" command: stream-id={}'
        logging.info(fmt.format(cmd.stream_id))
        stream_state = self._get_stream_state(cmd.stream_id)

        if type(stream_state) is not _LttngLiveViewerSessionDataStreamState:
            raise UnexpectedInput(
                'Stream with ID {} is not a data stream'.format(cmd.stream_id)
            )

        if stream_state.cur_index_entry is None:
            # The viewer is done reading this stream
            status = _LttngLiveViewerGetNextDataStreamIndexEntryReply.Status.HUP

            # Dummy data stream index entry to use with the `HUP` status
            # (the reply needs one, but the viewer ignores it)
            index_entry = _LttngDataStreamIndexEntry(0, 0, 0, 0, 0, 0, 0)

            return _LttngLiveViewerGetNextDataStreamIndexEntryReply(
                status, index_entry, False, False
            )

        # The viewer only checks the `has_new_metadata` flag if the
        # reply's status is `OK`, so we need to provide an index here
        has_new_metadata = stream_state.tracing_session_state.has_new_metadata
        status = _LttngLiveViewerGetNextDataStreamIndexEntryReply.Status.OK
        reply = _LttngLiveViewerGetNextDataStreamIndexEntryReply(
            status, stream_state.cur_index_entry, has_new_metadata, False
        )
        stream_state.goto_next_index_entry()
        return reply

    def _handle_get_data_stream_packet_data_command(self, cmd):
        fmt = 'Handling "get data stream packet data" command: stream-id={}, offset={}, req-length={}'
        logging.info(fmt.format(cmd.stream_id, cmd.offset, cmd.req_length))
        stream_state = self._get_stream_state(cmd.stream_id)
        data_response_length = cmd.req_length

        if type(stream_state) is not _LttngLiveViewerSessionDataStreamState:
            raise UnexpectedInput(
                'Stream with ID {} is not a data stream'.format(cmd.stream_id)
            )

        if stream_state.tracing_session_state.has_new_metadata:
            status = _LttngLiveViewerGetDataStreamPacketDataReply.Status.ERROR
            return _LttngLiveViewerGetDataStreamPacketDataReply(
                status, bytes(), True, False
            )

        if self._max_query_data_response_size:
            # Enforce a server side limit on the query requested length.
            # To ensure that the transaction terminate take the minimum of both
            # value.
            data_response_length = min(
                cmd.req_length, self._max_query_data_response_size
            )
            fmt = 'Limiting "get data stream packet data" command: req-length={} actual response size={}'
            logging.info(fmt.format(cmd.req_length, data_response_length))

        data = stream_state.data_stream.get_data(cmd.offset, data_response_length)
        status = _LttngLiveViewerGetDataStreamPacketDataReply.Status.OK
        return _LttngLiveViewerGetDataStreamPacketDataReply(status, data, False, False)

    def _handle_get_metadata_stream_data_command(self, cmd):
        fmt = 'Handling "get metadata stream data" command: stream-id={}'
        logging.info(fmt.format(cmd.stream_id))
        stream_state = self._get_stream_state(cmd.stream_id)

        if type(stream_state) is not _LttngLiveViewerSessionMetadataStreamState:
            raise UnexpectedInput(
                'Stream with ID {} is not a metadata stream'.format(cmd.stream_id)
            )

        if stream_state.is_sent:
            status = _LttngLiveViewerGetMetadataStreamDataContentReply.Status.NO_NEW
            return _LttngLiveViewerGetMetadataStreamDataContentReply(status, bytes())

        stream_state.is_sent = True
        status = _LttngLiveViewerGetMetadataStreamDataContentReply.Status.OK
        return _LttngLiveViewerGetMetadataStreamDataContentReply(
            status, stream_state.metadata_stream.data
        )

    def _handle_get_new_stream_infos_command(self, cmd):
        fmt = 'Handling "get new stream infos" command: ts-id={}'
        logging.info(fmt.format(cmd.tracing_session_id))

        # As of this version, all the tracing session's stream infos are
        # always given to the viewer when sending the "attach to tracing
        # session" reply, so there's nothing new here. Return the `HUP`
        # status as, if we're handling this command, the viewer consumed
        # all the existing data streams.
        status = _LttngLiveViewerGetNewStreamInfosReply.Status.HUP
        return _LttngLiveViewerGetNewStreamInfosReply(status, [])

    def _handle_get_tracing_session_infos_command(self, cmd):
        logging.info('Handling "get tracing session infos" command.')
        infos = [
            tss.tracing_session_descriptor.info for tss in self._ts_states.values()
        ]
        infos.sort(key=lambda info: info.name)
        return _LttngLiveViewerGetTracingSessionInfosReply(infos)

    def _handle_create_viewer_session_command(self, cmd):
        logging.info('Handling "create viewer session" command.')
        status = _LttngLiveViewerCreateViewerSessionReply.Status.OK

        # This does nothing here. In the LTTng relay daemon, it
        # allocates the viewer session's state.
        return _LttngLiveViewerCreateViewerSessionReply(status)


# An LTTng live TCP server.
#
# On creation, it binds to `localhost` with an OS-assigned TCP port. It writes
# the decimal TCP port number to a temporary port file.  It renames the
# temporary port file to `port_filename`.
#
# `tracing_session_descriptors` is a list of tracing session descriptors
# (`LttngTracingSessionDescriptor`) to serve.
#
# This server accepts a single viewer (client).
#
# When the viewer closes the connection, the server's constructor
# returns.
class LttngLiveServer:
    def __init__(
        self, port_filename, tracing_session_descriptors, max_query_data_response_size
    ):
        logging.info('Server configuration:')

        logging.info('  Port file name: `{}`'.format(port_filename))

        if max_query_data_response_size is not None:
            logging.info(
                '  Maximum response data query size: `{}`'.format(
                    max_query_data_response_size
                )
            )

        for ts_descr in tracing_session_descriptors:
            info = ts_descr.info
            fmt = '  TS descriptor: name="{}", id={}, hostname="{}", live-timer-freq={}, client-count={}, stream-count={}:'
            logging.info(
                fmt.format(
                    info.name,
                    info.tracing_session_id,
                    info.hostname,
                    info.live_timer_freq,
                    info.client_count,
                    info.stream_count,
                )
            )

            for trace in ts_descr.traces:
                logging.info('    Trace: path="{}"'.format(trace.path))

        self._ts_descriptors = tracing_session_descriptors
        self._max_query_data_response_size = max_query_data_response_size
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._codec = _LttngLiveViewerProtocolCodec()

        # Port 0: OS assigns an unused port
        serv_addr = ('localhost', 0)
        self._sock.bind(serv_addr)
        self._write_port_to_file(port_filename)

        try:
            self._listen()
        finally:
            self._sock.close()
            logging.info('Closed connection and socket.')

    @property
    def _server_port(self):
        return self._sock.getsockname()[1]

    def _recv_command(self):
        data = bytes()

        while True:
            logging.info('Waiting for viewer command.')
            buf = self._conn.recv(128)

            if not buf:
                logging.info('Client closed connection.')

                if data:
                    raise UnexpectedInput(
                        'Client closed connection after having sent {} command bytes.'.format(
                            len(data)
                        )
                    )

                return

            logging.info('Received data from viewer: length={}'.format(len(buf)))

            data += buf

            try:
                cmd = self._codec.decode(data)
            except struct.error as exc:
                raise UnexpectedInput('Malformed command: {}'.format(exc)) from exc

            if cmd is not None:
                logging.info(
                    'Received command from viewer: cmd-cls-name={}'.format(
                        cmd.__class__.__name__
                    )
                )
                return cmd

    def _send_reply(self, reply):
        data = self._codec.encode(reply)
        logging.info(
            'Sending reply to viewer: reply-cls-name={}, length={}'.format(
                reply.__class__.__name__, len(data)
            )
        )
        self._conn.sendall(data)

    def _handle_connection(self):
        # First command must be "connect"
        cmd = self._recv_command()

        if type(cmd) is not _LttngLiveViewerConnectCommand:
            raise UnexpectedInput(
                'First command is not "connect": cmd-cls-name={}'.format(
                    cmd.__class__.__name__
                )
            )

        # Create viewer session (arbitrary ID 23)
        logging.info(
            'LTTng live viewer connected: version={}.{}'.format(cmd.major, cmd.minor)
        )
        viewer_session = _LttngLiveViewerSession(
            23, self._ts_descriptors, self._max_query_data_response_size
        )

        # Send "connect" reply
        self._send_reply(
            _LttngLiveViewerConnectReply(viewer_session.viewer_session_id, 2, 10)
        )

        # Make the viewer session handle the remaining commands
        while True:
            cmd = self._recv_command()

            if cmd is None:
                # Connection closed (at an expected location within the
                # conversation)
                return

            self._send_reply(viewer_session.handle_command(cmd))

    def _listen(self):
        logging.info('Listening: port={}'.format(self._server_port))
        # Backlog must be present for Python version < 3.5.
        # 128 is an arbitrary number since we expect only 1 connection anyway.
        self._sock.listen(128)
        self._conn, viewer_addr = self._sock.accept()
        logging.info(
            'Accepted viewer: addr={}:{}'.format(viewer_addr[0], viewer_addr[1])
        )

        try:
            self._handle_connection()
        finally:
            self._conn.close()

    def _write_port_to_file(self, port_filename):
        # Write the port number to a temporary file.
        with tempfile.NamedTemporaryFile(mode='w', delete=False) as tmp_port_file:
            print(self._server_port, end='', file=tmp_port_file)

        # Rename temporary file to real file
        os.replace(tmp_port_file.name, port_filename)
        logging.info(
            'Renamed port file: src-path="{}", dst-path="{}"'.format(
                tmp_port_file.name, port_filename
            )
        )


# A tracing session descriptor.
#
# In the constructor, `traces` is a list of LTTng traces (`LttngTrace`
# objects).
class LttngTracingSessionDescriptor:
    def __init__(
        self, name, tracing_session_id, hostname, live_timer_freq, client_count, traces
    ):
        for trace in traces:
            if name not in trace.path:
                fmt = 'Tracing session name must be part of every trace path (`{}` not found in `{}`)'
                raise ValueError(fmt.format(name, trace.path))

        self._traces = traces
        stream_count = sum([len(t) + 1 for t in traces])
        self._info = _LttngLiveViewerTracingSessionInfo(
            tracing_session_id,
            live_timer_freq,
            client_count,
            stream_count,
            hostname,
            name,
        )

    @property
    def traces(self):
        return self._traces

    @property
    def info(self):
        return self._info


def _tracing_session_descriptors_from_arg(string):
    # Format is:
    #     NAME,ID,HOSTNAME,FREQ,CLIENTS,TRACEPATH[,TRACEPATH]...
    parts = string.split(',')
    name = parts[0]
    tracing_session_id = int(parts[1])
    hostname = parts[2]
    live_timer_freq = int(parts[3])
    client_count = int(parts[4])
    traces = [LttngTrace(path) for path in parts[5:]]
    return LttngTracingSessionDescriptor(
        name, tracing_session_id, hostname, live_timer_freq, client_count, traces
    )


def _loglevel_parser(string):
    loglevels = {'info': logging.INFO, 'warning': logging.WARNING}
    if string not in loglevels:
        msg = "{} is not a valid loglevel".format(string)
        raise argparse.ArgumentTypeError(msg)
    return loglevels[string]


if __name__ == '__main__':
    logging.basicConfig(format='# %(asctime)-25s%(message)s')
    parser = argparse.ArgumentParser(
        description='LTTng-live protocol mocker', add_help=False
    )
    parser.add_argument(
        '--log-level',
        default='warning',
        choices=['info', 'warning'],
        help='The loglevel to be used.',
    )

    loglevel_namespace, remaining_args = parser.parse_known_args()
    logging.getLogger().setLevel(_loglevel_parser(loglevel_namespace.log_level))

    parser.add_argument(
        '--port-filename',
        help='The final port file. This file is present when the server is ready to receive connection.',
        required=True,
    )
    parser.add_argument(
        '--max-query-data-response-size',
        type=int,
        help='The maximum size of control data response in bytes',
    )
    parser.add_argument(
        'sessions',
        nargs="+",
        metavar="SESSION",
        type=_tracing_session_descriptors_from_arg,
        help='A session configuration. There is no space after comma. Format is: NAME,ID,HOSTNAME,FREQ,CLIENTS,TRACEPATH[,TRACEPATH]....',
    )
    parser.add_argument(
        '-h',
        '--help',
        action='help',
        default=argparse.SUPPRESS,
        help='Show this help message and exit.',
    )

    args = parser.parse_args(args=remaining_args)
    try:
        LttngLiveServer(
            args.port_filename, args.sessions, args.max_query_data_response_size
        )
    except UnexpectedInput as exc:
        logging.error(str(exc))
        print(exc, file=sys.stderr)
        sys.exit(1)
