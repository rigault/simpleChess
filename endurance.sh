#!/bin/bash
jeu="rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR+b+KQkq"
#jeu="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR+w+KQkq"
#jeu="3k3r/8/8/8/8/8/8/4K3+b+--"
strFini="ONGOING"
round=1
while [ $strFini != "END" ]; do
   sudo ./chess.cgi -r $jeu > temp
   jeu=$(cat temp | grep "fen: " | sed "s/fen: //")
   strFini=$(cat temp | grep "etat: " | sed "s/etat: //")
   cat temp
   echo "Round: $round"
   (( round += 1 ))
done
