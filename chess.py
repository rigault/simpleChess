#!/usr/bin/env python3
import sys, os, subprocess, json
import vt
from subprocess import Popen, PIPE, STDOUT
WHITE = (-1)
BLACK = 1
HELP = "usage: ./chess.py -e|-c|-p|-m|-t"
fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR+w+KQkq+-+0+0"
curl1 = 'curl -s "http://23.251.143.190/cgi-bin/chess.cgi?reqType=2&level=4&alea=0&notrans&fen='
curl2 = 'curl -s "http://23.251.143.190/cgi-bin/chess.cgi?reqType=2&level=4&alea=0&fen='
level = 4
exp = 22

unicode = {"p": "♟", 'n' : "♞", "b":"♝", "r":"♜", "q":"♛", "k":"♚"};

def printGame (fen) :
   print (" a  b  c  d  e  f  g  h")
   l = 8
   normal = True
   for x in fen :
      if x == "/" : 
         sys.stdout.write (vt.DEFAULT_COLOR)
         print (" ", l)
         normal =  ((l % 2) == 1)
         l -= 1
      elif x.isnumeric () :
         for k in range (int (x)) :
            sys.stdout.write ((vt.BG_CYAN if normal else vt.BG_BLACK) + "   ") 
            normal = not normal
      elif not x.isalnum () : break
      else :
         sys.stdout.write (vt.BG_CYAN if normal else vt.BG_BLACK) 
         sys.stdout.write (vt.C_WHITE if x.isupper () else vt.C_RED)
         sys.stdout.write (unicode [x.lower()] + "  " + vt.C_WHITE)
         normal = not normal
   sys.stdout.write (vt.DEFAULT_COLOR + vt.C_WHITE)
   print (" ", l)

def play (command) :
   maxTr = maxEval = nEvalCall = nbtrta = 0
   score = "-"
   p = Popen (command, shell=True, stdin=PIPE, stdout=PIPE, stderr=STDOUT, close_fds=True).stdout.read ().decode ('utf-8')
   game = p.split ('{', 1)[0] 
   jsonText = "{" + p.split ('{', 1)[1] # un seul split apres {
   j = json.loads (jsonText)
   if "score" in j : score = j ["score"]
   if "nEvalCall" in j : nEvalCall = j ["dump"]["nEvalCall"]
   if "nbTra" in j : nbtrta = j ["dump"]["nbTrTa"]
   if nEvalCall > maxEval : maxEval = nEvalCall
   if nbtrta > maxTr : maxTr = nbtrta
   # os.system ("clear")
   print (game)
   for x in j : 
      if (str(j[x]).strip()) != "" :  print (x, "=", j[x])
   return j ["time"], j["fen"], score

def endurance (fen, level, exp) :
   score = "-"
   time1 = 0
   time2 = 0
   while score == "-" :
      # les blancs jouent
      t1, fen, score = play ("./chess.cgi -vnno " + fen + " " + str(level) + " " + str (exp))
      time1 += t1
      if score == "-" :
         # les noirs jouent
         t2, fen, score = play ("./chess.cgi -vono " + fen + " " + str(level) + " " + str (exp))
         time2 += t2
   return time1 + time2

def comp (fen, level, exp) :
   nEgal = nDiff = 0
   score = "-"
   time1 = 0
   time2 = 0
   while score == "-" :
      t1, fen1, score = play ("./chessold.cgi -vono " + fen + " " + str(level) + " " + str (exp))
      t2, fen2, score = play ("./chess.cgi -vono " + fen + " " + str(level) + " " + str (exp))
      #t1, fen1, score = play (curl1 + fen + '"')
      #t2, fen2, score = play (curl2 + fen + '"')
      
      if fen1 == fen2 : 
         print ("equal")
         nEgal += 1
      else : 
         print ("different in chess.py com")
         exit ()
         nDiff +=1
      fen = fen2
      time1 += t1
      time2 += t2
   print ("nEgal = ", nEgal, " ; nDiff = ", nDiff)
   print ("Time1 = ", time1, " Time2 = ", time2) 

def play2 (fen, leveli, exp) :
   score = "-"
   car = input ("\nb)lack or w)hite ? : ").upper ()
   color = WHITE if (car == "W") else BLACK
   print ("You have the: ",  "Whites" if color == WHITE else "Blacks");
   player = (color == WHITE) 
   if player : play ("./chess.cgi -M " + fen);
   while score == "-" :
      if player :
         move = input ("gamer move (ex : e2e4 or Pe7-e8=Q) : ");
         t1, fen, score = play ("./chess.cgi -M " + fen + " " + str(level) + " " + move);

      else : # computer
         t1, fen, score = play ("./chess.cgi -von " + fen + " " + str(level) + " " + str (exp))
      player = not player;

def mesure (fen, level) :
   strRep = ""
   for exp in range (10, 20) :
      time = endurance (fen, level, exp)
      strRep += "Exp = " + str (exp) + ", Time = " + str (time) + "\n"
   print ("------RESULTAT-Level " + str (level) + "---")
   print (strRep)
   
if len (sys.argv) <= 1 : print (HELP)
else :
   if sys.argv[1] == "-e" : print ("Total Time : " + str (endurance (fen, level, exp)))
   elif sys.argv[1] == "-c" : comp (fen, level, exp)
   elif sys.argv[1] == "-p" : play2 (fen, level, exp)
   elif sys.argv[1] == "-m" : mesure (fen, level)
   elif sys.argv[1] == "-t" : printGame (fen)
   else : print (HELP)
