/*   Pour produire la doc sur les fonctions : grep "\/\*" playfen.c | sed 's/^\([a-zA-Z]\)/\n\1/' */
/*   Rejoue un jeu a partir des logs Attention premier champ : fen */
/*   ./playfen <fichier log> */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define SEP ";"
#include "chessUtil.h"


void process (FILE *fe) { /* */
   /* lit le fichier FEN */
   /* ce fichier est au format CSV : FENstring ; dep ; commentaire */
   /* dep contient le deplacement en notation algebrique complete Xe2-e4[=Y] | O-O | O-O-O */
   /* X : piece joue. Y : promotion,  O-O : petit roque,  O-O-O : grand roque */
   char line [MAXLEN];
   char *sFEN, *ptDep, *ptEval, *ptComment;
   int cpt50, nb;
   int eval = 0;
   TGAME jeu;
   int len;
   while (fgets (line, MAXLEN, fe) != NULL) {
      len = strlen (line);
      if ((sFEN = strtok (line, SEP)) != NULL) {
         if ((ptDep = strtok (NULL, SEP)) != NULL) {
            if ((ptEval = strtok (NULL, SEP)) != NULL) {
               eval = atoi (ptEval);
               ptComment = strtok (NULL, "\n");
            }
         }

      }
      fenToGame (sFEN, jeu, &cpt50, &nb);
      printGame (jeu, eval);
      printf ("dep: %s\n", ptDep);
      printf ("Comment: %s\n", (ptComment != NULL) ? ptComment : "");
   }
}


int main (int argc, char *argv []) { /* */
   FILE *fe;
   if (argc != 2) {
      fprintf (stderr, "Usage: %s <filename>\n", argv [0]);
      exit (EXIT_FAILURE);
   }
   if ((fe = fopen (argv [1], "r")) == NULL) {
      fprintf (stderr, "file: %s not found\n", argv [1]);
      exit (EXIT_FAILURE);
   }
   process (fe);
   fclose (fe);
}
