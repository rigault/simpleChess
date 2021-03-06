chess.cgi: chess.o chessUtil.o tbprobe.o syzygy.o
	gcc -pthread -O3 -Wall -D TB_NO_THREADS -I.. syzygy.o tbprobe.o chessUtil.o chess.o  -o chess.cgi 

chess.o: chess.c chessUtil.h 
	gcc -c -O3 -Wall -pedantic -std=c99 chess.c

chessUtil.o: chessUtil.c chessUtil.h
	gcc -c -O3 -Wall -Wextra -pedantic -std=c11 chessUtil.c

syzygy.o: syzygy.c syzygy.h
	gcc -c -O3 -Wall -Wextra -pedantic -std=c11 syzygy.c

tbprobe.o: tbprobe.c
	gcc -c -O3 -Wall -Wextra -pedantic -std=c11 -D TB_NO_THREADS -D TB_NO_HW_POP_COUNT -I.. tbprobe.c

clean:
	rm -f *.o
	cp chess.cgi /var/www/html/cgi-bin/.

