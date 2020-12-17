#!/bin/bash
jeu="rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR+b+KQkq"
#jeu="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR+w+KQkq"
#jeu="3k3r/8/8/8/8/8/8/4K3+b+--"
strFini="ONGOING"
round=1
cpt50=0
while [ $strFini != "END" ] && (($cpt50 <= 50)); do
   sudo ./chess.cgi -r $jeu > temp
   jeu=$(cat temp | grep "fen: " | sed "s/fen: //")
   strFini=$(cat temp | grep "etat: " | sed "s/etat: //")
   move=$(cat temp | grep "move: " | sed "s/move: //")
   cat temp
   (( round += 1 ))
   if [ ${move:0:1} == "P" ] || [ ${move:3:1} == "x" ]; then
      cpt50=0;
   else 
      (( cpt50 += 1))
   fi
   echo "Round: $round, cpt50: $cpt50"
done
