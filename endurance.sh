#!/bin/bash
fen="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR+w+KQkq+-+0+0"
score="-"
while [ $score == "-" ]; do
   sudo ./chess.cgi -v $fen | tr -d '\000' > temp # il faut virer les null
   # clear
   fen=`grep ^\"fen temp | cut -d: -f 2 | tr -d \ \"\,`   
   score=`grep ^\"score temp | cut -d: -f 2 | tr -d \ \"\,`
   cat temp
   #read
done
