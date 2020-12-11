clang -pthread -std=gnu99 -O2 -Wall -D TB_NO_THREADS -D TB_NO_HW_POP_COUNT -I.. syzygy.c tbprobe.c chessUtil.c chess.c -o chess.cgi
