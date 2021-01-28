#!/bin/bash
fen="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR+w+KQkq+-+0+0"
score="-"
((maxTr = 0))
((maxEval = 0))
while [ $score == "-" ]; do
   # les blances jouent
   sudo ./chess.cgi -v $fen 4 | tr -d '\000' > temp # il faut virer les null
   # clear
   fen=`grep ^\"fen temp | cut -d: -f 2 | tr -d \ \"\,`   
   score=`grep ^\"score temp | cut -d: -f 2 | tr -d \ \"\,`
   move1=`grep ^\"computePlayC temp | cut -d: -f 2 | tr -d \ \"\,`
   nEvalCall=`grep ^\"nEvalCall temp | cut -d: -f 2 | tr -d \ \"\,`
   #nbtrta=`grep nbTrTa= temp | cut -d= -f 2 | sed -e s/nbMatchTrans.*//`
   if (($nEvalCall > $maxEval)); then
      maxEval=$nEvalCall
   fi 
   #if (($nbtrta > $maxTr)); then
   #   maxTr=$nbtrta
   #fi 
   echo maxEval=$maxEval
   #echo maxTr=$maxTr
   cat temp
   if [[ $score == "-" ]]; then
      # les noirs jouent
      sudo ./chess.cgi -vn $fen 4 | tr -d '\000' > temp # il faut virer les null
      fen=`grep ^\"fen temp | cut -d: -f 2 | tr -d \ \"\,`   
      move2=`grep ^\"computePlayC temp | cut -d: -f 2 | tr -d \ \"\,`
      score=`grep ^\"score temp | cut -d: -f 2 | tr -d \ \"\,`
      cat temp
   fi
   #read
done
