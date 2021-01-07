#!/bin/bash
declare -a StringArray=("chess.c" "syzygy.c" "chessUtil.c")
 
for file in ${StringArray[@]}; do
   echo "Fichier $file"
   l=${#file}   
   for ((i = 0; i < $l; i++)); do
      echo -n "-"
   done
   echo "--------"
   grep "\/\*" $file | sed 's/^\([a-zA-Z]\)/\n\1/'
   echo; echo
done
