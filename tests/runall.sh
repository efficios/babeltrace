#!/bin/bash

TESTDIR=$(dirname $0)
DIR=$(readlink -f ${TESTDIR})
BABELTRACE_BIN=${DIR}/../converter/babeltrace
CTF_TRACES=${DIR}/ctf-traces

function test_check_success ()
{
	if [ $? -ne 0 ] ; then
		return 1
	else
		return 0
	fi
}

function test_check_fail ()
{
	if [ $? -eq 0 ] ; then
		return 1
	else
		return 0
	fi
}

function run_babeltrace ()
{
	${BABELTRACE_BIN} $* > /dev/null 2>&1
	return $?
}

function print_test_result ()
{
	if [ $# -ne 3 ] ; then
		echo "Invalid arguments provided"
		exit 1
	fi

	if [ ${2} -eq 0 ] ; then
		echo -n "ok"
	else
		echo -n "not ok"
	fi
	echo -e " "${1}" - "${3}
}

successTraces=(${CTF_TRACES}/succeed/*)
failTraces=(${CTF_TRACES}/fail/*)
testCount=$((2 + ${#successTraces[@]} + ${#failTraces[@]}))

currentTestIndex=1
echo -e 1..${testCount}

#run babeltrace, expects success
run_babeltrace
test_check_success
print_test_result $((currentTestIndex++)) $? "Running babeltrace without arguments"

#run babeltrace with a bogus argument, expects failure
run_babeltrace --bogusarg
test_check_fail
print_test_result $((currentTestIndex++)) $? "Running babeltrace with a bogus argument"

for tracePath in ${successTraces[@]}; do
	run_babeltrace ${tracePath}
	test_check_success
	print_test_result $((currentTestIndex++)) $? "Running babeltrace with trace ${tracePath}"
done

for tracePath in ${failTraces[@]}; do
	run_babeltrace ${tracePath}
	test_check_fail
	print_test_result $((currentTestIndex++)) $? "Running babeltrace with trace ${tracePath}"
done

exit 0
