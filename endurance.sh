#!/bin/bash
jeu="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR+w+KQkqi 4"
#jeu="3k3r/8/8/8/8/8/8/4K3+b+--"
strFini="ONGOING"
round=1
cpt50=0
declare -a strStatus=("Not exist" "exist" "echec au roi" "invalid" "mat" "pat");

while [ $strFini != "END" ] && (($cpt50 <= 50)); do
   sudo ./chess.cgi -r $jeu > temp
   jeu=$(cat temp | grep "fen: " | sed "s/fen: //")
   strFini=$(cat temp | grep "etat: " | sed "s/etat: //")
   move=$(cat temp | grep "move: " | sed "s/move: //")
   computerStatus=$(cat temp | grep "computerStatus: " | sed "s/.*computerStatus: //" | sed "s/,.*//")
   playerStatus=$(cat temp | grep "playerStatus: " | sed "s/.*playerStatus: //")
   cat temp
   (( round += 1 ))
   if [ ${move:0:1} == "P" ] || [ ${move:3:1} == "x" ]; then
      cpt50=0;
   else 
      (( cpt50 += 1))
   fi
   echo "Round: $round, cpt50: $cpt50"
   if ((playerStatus >= 2)); then
      echo "Player status: "${strStatus[$playerStatus]}
   fi
   if ((computerStatus >= 2)); then
      echo "Computer status: "${strstatus[$playerStatus]}
   fi
done
