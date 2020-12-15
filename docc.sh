#!/bin/bash
grep "\/\*" chess.c | sed 's/^\([a-zA-Z]\)/\n\1/'
grep "\/\*" syzygy.c | sed 's/^\([a-zA-Z]\)/\n\1/'
grep "\/\*" chessUtil.c | sed 's/^\([a-zA-Z]\)/\n\1/'
grep "\/\*" buildlistsimple.c | sed 's/^\([a-zA-Z]\)/\n\1/'
