#!/bin/python
import sys, os, subprocess, json
from subprocess import Popen, PIPE, STDOUT
fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR+w+KQkq+-+0+0"
score = "-"
maxTr = 0
maxEval = 0
while  score == "-" :
   # les blancs jouent
   command = "sudo ./chess.cgi -vnn " + fen + " 3"
   p = Popen(command, shell=True, stdin=PIPE, stdout=PIPE, stderr=STDOUT, close_fds=True).stdout.read ().decode ('utf-8')
   jsonText = "{" + p.split ('{', 1)[1] # un seul split apres {
   j = json.loads (jsonText)
   fen = j ["fen"]
   score = j ["score"]
   nEvalCall = j ["nEvalCall"]
   nbtrta = j ["dump"]["nbTrTa"]
   if nEvalCall > maxEval : maxEval = nEvalCall
   if nbtrta > maxTr : maxTr = nbtrta
   
   print ("maxEval = ", maxEval)
   print ("maxTr = ", maxTr)
   print (p)
   if  score == "-" :
      # les noirs jouent
      command = "sudo ./chess.cgi -von " + fen + " 3"
      p = Popen(command, shell=True, stdin=PIPE, stdout=PIPE, stderr=STDOUT, close_fds=True).stdout.read ().decode ('utf-8')
      jsonText = "{" + p.split ("{", 1)[1] # un seul split apres {
      j = json.loads (jsonText)
      fen = j ["fen"]
      score = j ["score"]
      print (p)
