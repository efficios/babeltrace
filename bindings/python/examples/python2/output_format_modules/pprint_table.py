# pprint_table.py
#
# This module is used to pretty-print a table
# Adapted from
# http://ginstrom.com/scribbles/2007/09/04/pretty-printing-a-table-in-python/

import sys

def get_max_width(table, index):
	"""Get the maximum width of the given column index"""

	return max([len(str(row[index])) for row in table])


def pprint_table(table, nbLeft=1, out=sys.stdout):
	"""
	Prints out a table of data, padded for alignment
	@param table: The table to print. A list of lists.
	Each row must have the same number of columns.
	@param nbLeft: The number of columns aligned left
	@param out: Output stream (file-like object)
	"""

	col_paddings = []

	for i in range(len(table[0])):
		col_paddings.append(get_max_width(table, i))

	for row in table:
		# left cols
		for i in range(nbLeft):
			print >> out, str(row[i]).ljust(col_paddings[i] + 1),
		# rest of the cols
		for i in range(nbLeft, len(row)):
			col = str(row[i]).rjust(col_paddings[i] + 2)
			print >> out, col,
		print >> out
