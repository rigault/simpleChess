#!/bin/bash
fen="rnbqkbnr/pppppppp/8/8/2P5/8/PP1PPPPP/RNBQKBNR+b+KQkq+c3+0+0"
score="-"
echo $fen
while [ $score == "-" ]; do
   sudo ./chess.cgi -R $fen | tr -d '\000' > temp # il faut virer les null
   fen=`grep "fen:" temp | sed "s/fen: //g"`
   score=`grep "score:" temp | sed "s/score: //g"`
   cat temp
   echo $fen
done
