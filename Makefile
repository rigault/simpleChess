chess.cgi: chess.o chessUtil.o tbprobe.o syzygy.o
	gcc -pthread -std=gnu99 -O2 -Wall -D TB_NO_THREADS -D TB_NO_HW_POP_COUNT -I.. syzygy.o tbprobe.c chessUtil.c chess.c -o chess.cgi

chess.o: chess.c chessUtil.h 
	gcc -c -Wall -pedantic -std=c99 chess.c

chessUtil.o: chessUtil.c chessUtil.h
	gcc -c -Wall -Wextra -pedantic -std=c99 chessUtil.c

syzygy.o: syzygy.c syzygy.h
	gcc -c -Wall -Wextra -pedantic -std=c99 syzygy.c

tbprobe.o: tbprobe.c
	gcc -c -std=gnu99 -O2 -Wall -D TB_NO_THREADS -D TB_NO_HW_POP_COUNT -I.. tbprobe.c

release:
	rm -f *.o
	cp chess.cgi /var/www/html/cgi-bin/.

