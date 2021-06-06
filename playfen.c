/*! \mainpage Rejoue un jeu a partir des logs 
 * \section usage
 * \li./playfen <fichier log> [col]
 * \li premier champ : col (1 par défaut).*/

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEP ";"
#include "chessUtil.h"

/*! Lit le fichier FEN
 * \li ce fichier est au format CSV : FENstring ; dep ; commentaire
 * \li dep contient le deplacement en notation algebrique complete Xe2-e4[=Y] | O-O | O-O-O
 * \li X : piece jouee. Y : promotion,  O-O : petit roque,  O-O-O : grand roque
 * \li ignore les col-1 premieres colonnes */
static void process (FILE *fe, int col) { /* */
   char line [MAXLENGTH];
   char *sFEN, *ptDep, *ptEval, *ptComment;
   int cpt50, nb;
   int eval = 0;
   int nLine = 0;
   char ep [3];
   tGame_t jeu;
   while (fgets (line, MAXLENGTH, fe) != NULL) {
      nLine += 1;
      sFEN = strtok (line, SEP);
      if (sFEN == NULL) continue;
      for (int i = 1; i < col && (sFEN != NULL); i++) sFEN = strtok (NULL, SEP);   
      if (sFEN == NULL) continue;
      if ((ptDep = strtok (NULL, SEP)) != NULL) {
         if ((ptEval = strtok (NULL, SEP)) != NULL) {
            eval = strtol (ptEval, NULL, 10);
            ptComment = strtok (NULL, "\n");
         }
      }
      fenToGame (sFEN, jeu, ep, &cpt50, &nb);
      printf ("line: %d\n", nLine);
      printGame (jeu, eval);
      printf ("dep: %s\n", ptDep);
      printf ("Comment: %s\n", (ptComment != NULL) ? ptComment : "");
   }
}

/*! Programme principal playfen. Lit le fichier en argument et lance process */
int main (int argc, char *argv []) { /* */
   FILE *fe;
   if ((argc != 2) && (argc != 3)) {
      fprintf (stderr, "Usage: %s <filename> [col]\n", argv [0]);
      exit (EXIT_FAILURE);
   }
   if ((fe = fopen (argv [1], "r")) == NULL) {
      fprintf (stderr, "file: %s not found\n", argv [1]);
      exit (EXIT_FAILURE);
   }
   process (fe, ((argc == 3) ? strtol (argv [2], NULL, 10) : 1));
   fclose (fe);
}
