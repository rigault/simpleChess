#!/bin/bash
REQUETE="&reqType=2&level=4&fen=rnbqkbnr/ppp1pppp/8/3p4/6P1/5N2/PPPPPP1P/RNBQKB1R+b+KQkq"

echo "serveur local"
curl -s "http://127.0.0.1/cgi-bin/chess.cgi?"+$REQUETE

#echo "serveur sur google Cloud"
#curl -s "http://23.251.143.190/cgi-bin/chess.cgi?"+$REQUETE

echo "serveur local RENE"
curl -s "http://192.168.1.100/cgi-bin/chess.cgi?"+$REQUETE

