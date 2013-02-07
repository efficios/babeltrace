#!/bin/sh
# run Seeks tests
# With a trace than contains empty packets
./test-seeks ../ctf-traces/succeed/wk-heartbeat-u/ 1351532897586558519 1351532897591331194
# With a bigger trace
./test-seeks ../ctf-traces/succeed/lttng-modules-2.0-pre5/ 61334174524234 61336381998396

# run bitfield tests
./test-bitfield
