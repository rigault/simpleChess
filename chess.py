#!/usr/bin/env python3
import sys, os, subprocess, json
import vt
from subprocess import Popen, PIPE, STDOUT
WHITE = (-1)
BLACK = 1
HELP = "usage: ./chess.py -e|-c|-p"
fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR+w+KQkq+-+0+0"
level = 4
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
   return j["fen"], score

def endurance (fen, level) :
   score = "-"
   while score == "-" :
      # les blancs jouent
      fen, score = play ("./chess.cgi -vnno " + fen + " " + str(level))
      if score == "-" :
         # les noirs jouent
         fen, score = play ("./chess.cgi -vnno " + fen + " " + str(level))

   return score

def comp (fen, level) :
   nEgal = nDiff = 0
   score = "-"
   while score == "-" :
      fen1, score = play ("./chess.cgi -qnno " + fen + " " + str(level))
      fen2, score = play ("./chessOld.cgi -qnno " + fen + " " + str(level))
      if fen1 == fen2 : 
         print ("equal")
         nEgal += 1
      else : 
         print ("different")
         printGame (fen);
         exit ()
         nDiff +=1
      fen = fen1
   print ("nEgal = ", nEgal, " ; nDiff = ", nDiff); 

def play2 (fen, level) :
   score = "-"
   car = input ("\nb)lack or w)hite ? : ").upper ()
   color = WHITE if (car == "W") else BLACK
   print ("You have the: ",  "Whites" if color == WHITE else "Blacks");
   player = (color == WHITE) 
   if player : play ("./chess.cgi -M " + fen);
   while score == "-" :
      if player :
         move = input ("gamer move (ex : e2e4 or Pe7-e8=Q) : ");
         fen, score = play ("./chess.cgi -M " + fen + " " + str(level) + " " + move);

      else : # computer
         fen, score = play ("./chess.cgi -von " + fen + " " + str(level))
      player = not player;

if len (sys.argv) <= 1 : print (HELP)
else :
   if sys.argv[1] == "-e" : endurance (fen, level)
   elif sys.argv[1] == "-c" : comp (fen, level)
   elif sys.argv[1] == "-p" : play2 (fen, level)
   elif sys.argv[1] == "-t" : printGame (fen)
   else : print (HELP)
