#!/bin/bash

TESTDIR=$(dirname $0)
DIR=$(readlink -f ${TESTDIR})
BABELTRACE_BIN=${DIR}/../converter/babeltrace
CTF_TRACES=${DIR}/ctf-traces

function print_ok ()
{
	# Check if we are a terminal
	if [ -t 1 ]; then
		echo -e "\e[1;32mOK\e[0m"
	else
		echo -e "OK"
	fi
}

function print_fail ()
{
	# Check if we are a terminal
	if [ -t 1 ]; then
		echo -e "\e[1;31mFAIL\e[0m"
	else
		echo -e "FAIL"
	fi
}

function test_check ()
{
	if [ $? -ne 0 ] ; then
		print_fail
		return 1
	else
		print_ok
		return 0
	fi
}

function test_check_fail ()
{
	if [ $? -ne 1 ] ; then
		print_fail
		return 1
	else
		print_ok
		return 0
	fi
}

function run_babeltrace ()
{
	${BABELTRACE_BIN} $* > /dev/null 2>&1
	return $?
}

echo -e "Running test-bitfield..."
./test-bitfield
test_check
if [ $? -ne 0 ]; then
	exit 1
fi

#run babeltrace expects success
echo -e "Running babeltrace without argument..."
run_babeltrace
test_check
if [ $? -ne 0 ]; then
	exit 1
fi

for a in ${CTF_TRACES}/succeed/*; do
	echo -e "Running babeltrace for trace ${a}..."
	run_babeltrace ${a}
	test_check
	if [ $? -ne 0 ]; then
		exit 1
	fi
done

#run babeltrace expects failure
echo -e "Running babeltrace with bogus argument..."
run_babeltrace --bogusarg
test_check_fail
if [ $? -ne 0 ]; then
	exit 1
fi

for a in ${CTF_TRACES}/fail/*; do
	echo -e "Running babeltrace for trace ${a}..."
	run_babeltrace ${a}
	test_check_fail
	if [ $? -ne 0 ]; then
		exit 1
	fi
done

exit 0
