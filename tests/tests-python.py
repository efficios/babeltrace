#!/usr/bin/env python3
import unittest
import sys
from babeltrace import *

class TestBabeltracePythonModule(unittest.TestCase):

	def test_handle_decl(self):
		#Context creation, adding trace
		ctx = Context()
		trace_handle = ctx.add_trace(
			"ctf-traces/succeed/lttng-modules-2.0-pre5",
			"ctf")
		self.assertIsNotNone(trace_handle, "Error adding trace")

		#TraceHandle test
		ts_begin = trace_handle.get_timestamp_begin(ctx, CLOCK_REAL)
		ts_end = trace_handle.get_timestamp_end(ctx, CLOCK_REAL)
		self.assertGreater(ts_end, ts_begin, "Error get_timestamp from trace_handle")

		lst = ctf.get_event_decl_list(trace_handle, ctx)
		self.assertIsNotNone(lst, "Error get_event_decl_list")

		name = lst[0].get_name()
		self.assertIsNotNone(name)

		fields = lst[0].get_decl_fields(ctf.scope.EVENT_FIELDS)
		self.assertIsNotNone(fields, "Error getting FieldDecl list")

		if len(fields) > 0:
			self.assertIsNotNone(fields[0].get_name(),
				"Error getting name from FieldDecl")

		#Remove trace
		ctx.remove_trace(trace_handle)
		del ctx
		del trace_handle


	def test_iterator_event(self):
		#Context creation, adding trace
		ctx = Context()
		trace_handle = ctx.add_trace(
			"ctf-traces/succeed/lttng-modules-2.0-pre5",
			"ctf")
		self.assertIsNotNone(trace_handle, "Error adding trace")

		begin_pos = IterPos(SEEK_BEGIN)
		it = ctf.Iterator(ctx, begin_pos)
		self.assertIsNotNone(it, "Error creating iterator")

		event = it.read_event()
		self.assertIsNotNone(event, "Error reading event")

		handle = event.get_handle()
		self.assertIsNotNone(handle, "Error getting handle")

		context = event.get_context()
		self.assertIsNotNone(context, "Error getting context")

		name = ""
		while event is not None and name != "sched_switch":
			name = event.get_name()
			self.assertIsNotNone(name, "Error getting event name")

			ts = event.get_timestamp()
			self.assertGreaterEqual(ts, 0, "Error getting timestamp")

			cycles = event.get_cycles()
			self.assertGreaterEqual(cycles, 0, "Error getting cycles")

			if name == "sched_switch":
				scope = event.get_top_level_scope(ctf.scope.STREAM_PACKET_CONTEXT)
				self.assertIsNotNone(scope, "Error getting scope definition")

				field = event.get_field(scope, "cpu_id")
				prev_comm_str = field.get_uint64()
				self.assertEqual(ctf.field_error(), 0, "Error getting vec info")

				field_lst = event.get_field_list(scope)
				self.assertIsNotNone(field_lst, "Error getting field list")

				fname = field.field_name()
				self.assertIsNotNone(fname, "Error getting field name")

				ftype = field.field_type()
				self.assertIsNot(ftype, -1, "Error getting field type (unknown)")

			ret = it.next()
			self.assertGreaterEqual(ret, 0, "Error moving iterator")

			event = it.read_event()

		pos = it.get_pos()
		self.assertIsNotNone(pos._pos, "Error getting iterator position")

		ret = it.set_pos(pos)
		self.assertEqual(ret, 0, "Error setting iterator position")

		pos = it.create_time_pos(ts)
		self.assertIsNotNone(pos._pos, "Error creating time-based iterator position")

		del it, pos
		set_pos = IterPos(SEEK_TIME, ts)
		it = ctf.Iterator(ctx, begin_pos, set_pos)
		self.assertIsNotNone(it, "Error creating iterator with end_pos")


	def test_file_class(self):
		f = File("test-bitfield.c")
		self.assertIsNotNone(f, "Error opening file")
		f.close()


if __name__ == "__main__":
	unittest.main()
