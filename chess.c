/*   Pour produire la doc sur les fonctions : grep "\/\*" chess.c | sed 's/^\([a-zA-Z]\)/\n\1/' */
/*   Jeu d'echec */
/*   Noirs : 1 (Minuscules) */
/*   Blancs : -1 (Majuscules) */
/*   case libre 0 */
/*   mode CLI API : chess -i [FENgame] [profondeur] */
/*   mode CLI Raw : chess -r [FENgame] [profondeur] */
/*   mode CLI Test : chess -t */
/*   script CGI gérant une API restful (GET) avec les réponses au format JSON */
/*   fichiers associes : chess.log, chessB.fen, chessW.fen, chessUtil.c, syzygy.c, tbprobes.c tbcore.c et .h associes  */
/*   Structures de donnees essentielles */
/*     - jeu represente dans un table a 2 dimensions : TGAME sq64 */
/*     - liste de jeux qui est un tableau de jeux :  TLIST list */
/*     - nextL est un entier pointant sur le prochain jeux a inserer dans la liste */

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

#define HELP "Syntax; sudo ./chess.cgi -i|-r|-h|-t [<jeu au format FEN>] [profondeur]"

#define MAXPIECESSYZYGY 6
#define MAXTHREADS 128             // nombre max de thread
#define KINGINCHECKEVAL 1          // evaluation du gain d'un echec au roi..
#define MATE 1000000
#define MAXBUFFER 10000            // tampon de caracteres pour longues chaines
#define MAXLENGTH 255              // pour ligne

#include "chessUtil.h"
#include "syzygy.h"

struct sGetInfo {                  // description de la requete emise par le client
   char fenString [MAXLENGTH];     // le jeu
   int reqType;                    // le type de requete : 0 1 ou 2
   int level;                      // la profondeur de la recherche souhaitee
   char activeColor;               // 'b' black, 'w' whites
} getInfo = {"", 1, 3, 'b'};

// valorisation des pieces dans l'ordre  VOID PAWN KNIGHT BISHOP ROOK QUEEN KING CASTLEKING
// fou > cavalier, tour = fou + 2 pions, dame > fou + tour + pion
// pion a la valeur 8 suur les cotes, 9 sur les  colonnes centrales et plus importante quand avances 
// Le roi qui a roque est a le code 7, le roi normal code 6
// Voir fn evaluation
int val [] = {0, 1000, 3000, 3000, 5000, 9000, 0, 100};

int tEval [MAXTHREADS];

TGAME sq64 = {
   {-ROOK, -KNIGHT, -BISHOP, -QUEEN, -KING, -BISHOP, -KNIGHT, -ROOK},
   {-PAWN, -PAWN, -PAWN, -PAWN, -PAWN, -PAWN, -PAWN, -PAWN},
   {VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID},
   {VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID},
   {VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID},
   {VOID, VOID, VOID, VOID, VOID, VOID, VOID, VOID},
   {PAWN, PAWN, PAWN, PAWN, PAWN, PAWN, PAWN, PAWN},
   {ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK}
};

int fMaxDepth (int lev, struct sinfo info) { /* */
   /* renvoie la profondeur du jeu en fonction du niveau choisi et */
   /* de l'etat du jeu */
   int prod = info.nValidComputerPos * info.nValidGamerPos;
   return  (prod < 400) ? lev + 2 : ((prod < 800) ? lev + 1 : lev); 
}

bool LCkingInCheck (TGAME sq64, register int who, register int l, register int c) { /* */
   /* vrai si le roi situe case l, c est echec au roi */
   /*'who' est la couleur du roi qui est attaque */
   register int w, w1, w2, i, j, k;
   info.nLCKingInCheckCall += 1;

   // pion menace
   if (who == -1) {
      if (l < 7) {
         if (c < 7 && sq64 [l+1][c+1] == PAWN) return true;
         if (c > 0 && sq64 [l+1][c-1] == PAWN) return true;
      }
   }
   else { //  who == 1
      if (l > 0) {
         if (c < 7 && sq64 [l-1][c+1] == -PAWN) return true;
         if (c > 0 && sq64 [l-1][c-1] == -PAWN) return true;
      }
   } // fin if (who...
   w1 = -who * KING;
   w2 = -who * CASTLEKING;
   // roi adverse  menace
   if (l < 7 && (sq64 [l+1][c] == w1 || sq64 [l+1][c] == w2)) return true;
   if (l > 0 && (sq64 [l-1][c] == w1 || sq64 [l-1][c] == w2)) return true;
   if (c < 7 && (sq64 [l][c+1] == w1 || sq64 [l][c+1] == w2)) return true;
   if (c > 0 && (sq64 [l][c-1] == w1 || sq64 [l][c-1] == w2)) return true;
   if (l < 7 && c < 7 &&(sq64 [l+1][c+1] == w1 || sq64 [l+1][c+1] == w2)) return true;
   if (l < 7 && c > 0 &&(sq64 [l+1][c-1] == w1 || sq64 [l+1][c-1] == w2)) return true;
   if (l > 0 && c < 7 &&(sq64 [l-1][c+1] == w1 || sq64 [l-1][c+1] == w2)) return true;
   if (l > 0 && c > 0 &&(sq64 [l-1][c-1] == w1 || sq64 [l-1][c-1] == w2)) return true;

   w = -who * KNIGHT;
   // cavalier menace
   if (l < 7 && c < 6 && sq64 [l+1][c+2] == w) return true;
   if (l < 7 && c > 1 && sq64 [l+1][c-2] == w) return true;
   if (l < 6 && c < 7 && sq64 [l+2][c+1] == w) return true;
   if (l < 6 && c > 0 && sq64 [l+2][c-1] == w) return true;
   if (l > 0 && c < 6 && sq64 [l-1][c+2] == w) return true;
   if (l > 0 && c > 1 && sq64 [l-1][c-2] == w) return true;
   if (l > 1 && c < 7 && sq64 [l-2][c+1] == w) return true;
   if (l > 1 && c > 0 && sq64 [l-2][c-1] == w) return true;

   w1 = -who * QUEEN;
   w2 = -who * ROOK;
   // tour ou reine menace
   for (i = l+1; i < N; i++) {
      w = sq64 [i][c];
// printf ("Tour u Reine i=%d l=%d w %d w1 %d\n", i, l, w, w1); 
      if (w == w1 || w == w2) return true;
      if (w != 0) break;
   }
   for (i = l-1; i >= 0; i--) {
      w = sq64 [i][c];
      if (w == w1 || w == w2) return true;
      if (w != 0) break;
   }
   for (j = c+1; j < N; j++) {
      w = sq64 [l][j];
      if (w == w1 || w == w2) return true;
      if (w != 0) break;
   }
   for (j = c-1; j >= 0; j--) {
      w = sq64 [l][j];
      if (w == w1 || w == w2) return true;
      if (w != 0) break;
   }

   // fou ou reine menace
   w2 = -who * BISHOP;
   for (k = 0; k < MIN (7-l, 7-c); k++) { // vers haut droit
      w = sq64 [l+k+1][c+k+1];
      if (w == w1 || w == w2) return true;
      if (w != 0) break;
   }
   for (k = 0; k < MIN (7-l, c); k++) {// vers haut gauche
      w = sq64 [l+k+1][c-k-1];
      if (w == w1 || w == w2) return true;
      if (w != 0) break;
   }
   for (k = 0; k < MIN (l, 7-c); k++) { // vers bas droit
      w = sq64 [l-k-1][c+k+1];
      if (w == w1 || w == w2) return true;
      if (w != 0) break;
   }
   for (k = 0; k < MIN (l, c); k++) { // vers bas gauche
      w = sq64 [l-k-1] [c-k-1];
      if (w == w1 || w == w2) return true;
      if (w != 0) break;
   }

   return false;
}

#include "buildlistsimple.c"

bool fKingInCheck (TGAME sq64, int who) { /* */
   /* retourne vrai si 'who' est en echec */
   register int l, c;
   for (l = 0; l < N; l++)
      for (c = 0; c < N; c++) 
         if ((who * sq64 [l][c]) >= KING) { // match KING et CASTLEKING
            if (LCkingInCheck(sq64, who, l, c)) return true;
            else return false;
	 }
   return false;
}

bool kingCannotMove (TGAME sq64, register int who) { /* */
   /* vrai si le roi du joueur 'who' ne peut plus bouger sans se mettre echec au roi */
   /* 'who' est la couleur du roi who est attaque */
   /* on essaye tous les jeux possibles. Si dans tous les cas on est echec au roi */
   /* c'est perdu. Noter que si le roi a le trait et qu'il n'est pas echec au roi il est Pat */
   /* si le roi est echec au roi il est mat */
   register int k;
   TLIST list;
   register int maxList = buildList(sq64, who, list);
   for (k = 0; k < maxList; k++) {
      if (! fKingInCheck (list [k], who)) return false;
   }
   return true;
}

void updateInfo (TGAME sq64) { /* */
   /* met a jour l'objet info a partir de l'objet jeu */
   int l, c, v;
   info.gamerKingState = info.computerKingState = NOEXIST;
   info.castleComputer = info.castleGamer = false;
   info.nGamerPieces = info.nComputerPieces = 0;
   for (l = 0; l < N; l++) {
      for (c = 0; c < N; c++) {
         v = - sq64 [l][c] * info.gamerColor;
         if (v > 0) info.nComputerPieces += 1;
         else if (v < 0) info.nGamerPieces += 1;
         if (v == KING || v == CASTLEKING) {
            info.lComputerKing = l;
            info.cComputerKing = c;
            info.computerKingState = EXIST;
         }
         if (v == CASTLEKING) info.castleComputer = true;
         if (v == -KING || v == -CASTLEKING) {
            info.lGamerKing = l;
            info.cGamerKing = c;
            info.gamerKingState = EXIST;
         }
         if (v == -CASTLEKING) info.castleGamer = true;
      }
   }
   if (info.gamerKingState == EXIST) {
      if (LCkingInCheck(sq64, info.gamerColor, info.lGamerKing, info.cGamerKing))
         info.gamerKingState = ISINCHECK;
      if (kingCannotMove(sq64, info.gamerColor)) {
         if (info.gamerKingState == ISINCHECK) info.gamerKingState = ISMATE;
         else info.gamerKingState = ISPAT;
      }
   }
   if (info.computerKingState == EXIST) {
      if (LCkingInCheck(sq64, -info.gamerColor, info.lComputerKing, info.cComputerKing))
         info.computerKingState = ISINCHECK;
      if (kingCannotMove(sq64, -info.gamerColor)) {
         if (info.computerKingState == ISINCHECK) info.computerKingState = ISMATE;
         else info.computerKingState = ISPAT;
      }
   }
   info.nValidGamerPos = buildList(sq64, info.gamerColor, list);
   info.nValidComputerPos = buildList(sq64, -info.gamerColor, list);
}


int evaluation (TGAME sq64, register int who) { /* */
   /* fonction d'evaluation retournant MATE si Ordinateur gagne, */
   /* -MATE si joueur gagne, 0 si nul,... */
   register int l, c, v, eval;
   int lwho, cwho, ladverse, cadverse, dist;
   bool kingInCheck;
   lwho = cwho = ladverse = cadverse = 0;
   eval = 0;
   info.nEvalCall += 1;
   for (l = 0; l < N; l++) {
      for (c = 0; c < N; c++) {
         eval += ((v = sq64 [l][c]) > 0 ? val [v] : -val [-v]);
         switch (v) {
         case KING : case CASTLEKING :
            if (who == 1) { lwho = l; cwho = c; }
            else  { ladverse = l; cadverse = c; }
            break;
         case -KING: case -CASTLEKING :
            if (who == 1) { ladverse = l; cadverse = c; }
            else  {lwho = l; cwho = c;}
            break;
         case PAWN: case KNIGHT: // on privilégie les pions et cavaliers au centre
            if (c >= 2 && c <= 5 && l >= 2 && l <= 5) eval += 1;
            // eval += 5-l; // plus on est proche de la dame plus le pion a de la valeur
            break;
         case -PAWN: case -KNIGHT:
            if (c >= 2 && c <= 5 && l >= 2 && l <= 5) eval -= 1;
            // eval -= l-2;
            break;
         default:;
         }
     }
   }
   dist = 0;
   for (l = 0; l < N; l++) // on calcule la somme des distances au roi adverse
      for (c = 0; c < N; c++)
         if(sq64 [l][c]*who > 0)
             dist = dist + abs (cadverse -c ) + abs (ladverse - l);

   eval += -who*dist;
   if (LCkingInCheck(sq64, who, lwho, cwho)) return -who * MATE; // who ne peut pas jouer et se mettre en echec
   kingInCheck = LCkingInCheck(sq64, -who, ladverse, cadverse);
   if (kingCannotMove(sq64, -who)) { 
      if (kingInCheck) return who * MATE;
      else return 0; // Pat
   }
   if (kingInCheck) eval += who * KINGINCHECKEVAL;
   return eval;
}


int alphaBeta (TGAME sq64, int turn, int p, int refAlpha, int refBeta) { /* */
   /* le coeur du programme */
   TLIST list;
   int maxList = 0;
   int k, note;
   int val;
   int alpha = refAlpha;
   int beta = refBeta;
   note = evaluation(sq64, turn);
   if (info.calculatedMaxDepth < p) info.calculatedMaxDepth = p;

   // conditions de fin de jeu
   if (p >= info.maxDepth) return note;
   if (note == MATE) return note-p;    // -p pour favoriser le choix avec faible profondeur
   if (note == -MATE) return note+p;  // +p idem
   // pire des notes a ameliorer
   if (turn == 1) {
      val = MATE;
      maxList = buildList(sq64, -1, list);
      for (k = 0; k < maxList; k++) {
         note = alphaBeta (list [k], -1, p+1, alpha, beta);
         if (note < val) val = note;  // val = minimum...
         if (alpha > val) return val;
         if (val < beta) beta = val;  // beta = MIN (beta, val);
      }
   }
   else {
   // recherche du maximum
      val = -MATE;
      maxList = buildList(sq64, 1, list);
      for (k = 0; k < maxList; k++) {
         note = alphaBeta (list [k], 1, p+1, alpha, beta);
         if (note > val) val = note; // val = maximum...
         if (beta < val) return val;
         if (val > alpha) alpha = val; // alpha = max (alpha, val);
      }
   }
   return val;
}

void *fThread (void *arg) { /* */
   /* association des thread a alphabeta */
   long k = (long) arg;
   tEval [k] = alphaBeta (list [k], -info.gamerColor, 0, -MATE, MATE);
   pthread_exit (NULL);
}

int find (TGAME sq64, TGAME bestSq64, int *bestNote, int color) { /* */
   /* ordinateur joue renvoie le meilleur jeu possible et le nombre de jeux possibles */
   TGAME localSq64;
   int i, random;
   long k;
   pthread_t tThread [MAXTHREADS];
   int possible [MAXSIZELIST]; // tableau contenant les indice de jeux ayant la meilleure note
   char fen [MAXLEN] = "";
   
   *bestNote = 0; 
   nextL = buildList(sq64, color, list);
   strcpy (info.comment, "");

   
   if (nextL == 0) return 0;

   // Hasard Total
   if (getInfo.level == -1) {
      k = rand () % nextL; // renvoie un entier >= 0 et strictement inferieur a nextL
      memcpy (bestSq64, list [k], GAMESIZE);
      return nextL;
   }
   
   memcpy (localSq64, sq64, GAMESIZE);
   gameToFen (localSq64, fen, color, ' ', false);

   // recherche de fin de partie voir  https://syzygy-tables.info/
   if ((info.nGamerPieces + info.nComputerPieces) <= MAXPIECESSYZYGY) {
      strcat (fen," - - 0 0"); // idem
      if (syzygyRR (PATHTABLE, fen, &info.wdl, info.move, info.comment)) {
         moveGame (localSq64, color, info.move);
         memcpy (bestSq64, localSq64, GAMESIZE);
	 return nextL;
      }
   }
     
   // ouvertures
   if (opening ((color == 1) ? F_OUVB: F_OUVW, fen, info.comment, info.move)) {
      moveGame (localSq64, color, info.move);
      memcpy (bestSq64, localSq64, GAMESIZE);
      return nextL;
   }
 
   // recherche par la methode minimax Alphabeta
   *bestNote = - color * MATE;
   memcpy (bestSq64, list [0], GAMESIZE);
   for (k = 0; k < nextL; k++)
      if (pthread_create (&tThread [k], NULL, fThread, (void *) k)) {
         perror ("pthread_create");
         return EXIT_FAILURE;
      }
   for (k = 0; k < nextL; k++)
      if (pthread_join (tThread [k], NULL)) {
          perror ("pthread_join");
          return EXIT_FAILURE;
      }

   for (int k = 0; k < nextL; k++) printGame (list [k], tEval [k]);
   // tEval contient les évaluations de toutes les possibilites
   // recherche de la meilleure note
   for (k = 0; k < nextL; k++) {
      if (kingCannotMove (list [k], -color) && tEval [k] != color*MATE) tEval [k] = 0;
      if (color == 1 && tEval [k] > *bestNote)
         *bestNote = tEval [k];
      if (color == -1 && tEval [k] < *bestNote)
         *bestNote = tEval [k];
   }
 
   // construction de la liste des jeux aillant la meilleure note
   i = 0;
   for (k = 0; k < nextL; k++)
      if (tEval [k] == *bestNote)
         possible [i++] = k; // liste des indice de jeux ayant la meilleure note

   random  = rand () % i; // renvoie un entier >=  0 et strictement inferieur a i
   k = possible [random]; // indice du jeu choisi au hasard
   k = possible [0];      // deterministe A ENLEVER SI RANDOM PREFERE
   memcpy (bestSq64, list [k], GAMESIZE);

   return nextL;
}

void test (TGAME sq64, TLIST list) { /* */
   /* Pour tests unitaires */
   char fen [MAXLENGTH] = "4k2r/8/8/8/8/5p2/8/4K3 b - - 1 0 0";
   char move [32];
   // char comment [10000] = "";
   if (syzygyRR (PATHTABLE, fen, &info.wdl, move, info.comment)) {
      // moveGame(sq64, move);
      printf ("OK, comment: %s\n", info.comment);
      printf ("move : %s\n", move);
   }
   else printf ("Not found \n");
}

void computerPlay (TGAME sq64) { /* */
   /* prepare le lancement de la recherche avec find */
   time_t chrono;
   TGAME bestSq64;
   info.gamerColor = (getInfo.activeColor == 'b')? -1 : 1;
   updateInfo(sq64);
   if (info.computerKingState == NOEXIST || info.computerKingState == ISMATE) return; // le roi est pris... 
   info.note = evaluation(sq64, info.gamerColor);
   updateInfo(sq64);
   if (info.gamerKingState == ISINCHECK) {
      info.gamerKingState = UNVALIDINCHECK; // le joueur n'a pas le droit d'etre en echec
   }
   if (info.computerKingState != ISMATE && info.computerKingState != ISPAT && 
      info.computerKingState != NOEXIST && info.gamerKingState != UNVALIDINCHECK) {
      info.maxDepth = fMaxDepth (getInfo.level, info);
      chrono = time (NULL);

      // lancement de la recherche par l'ordi
      if ((info.nValidComputerPos = find (sq64, bestSq64, &info.evaluation, -info.gamerColor)) > 0) {  
         info.computeTime = (int) difftime (time (NULL), chrono);
         difference(sq64, bestSq64, -info.gamerColor, &info.lastCapturedByComputer, info.computerPlay);
         memcpy(sq64, bestSq64, GAMESIZE);
         info.note = evaluation(sq64, -info.gamerColor);
         updateInfo(sq64);
         if (fKingInCheck(sq64, -info.gamerColor))        // pas le droit d etre en echec apres avoir joue
            info.computerKingState = UNVALIDINCHECK;      // ou alors c'est Mat
      }
   }
}

void cgi () { /* */
   /* MODE CGI
   */
   char *str;
   char *env;
   char temp [MAXBUFFER];
   str = temp;
   char buffer [80] = "";
   srand (time (NULL)); // initialise le generateur aleatoire
   
   // log date heure et adresse IP du joueur
   time_t now = time (NULL);
   struct tm *timeNow = localtime (&now);
   strftime (buffer, 80, "%F; %T", timeNow);
   fprintf (flog, "%s; ", buffer);            // log de la date et du temps
   env = buffer;
   env = getenv ("REMOTE_ADDR");
   fprintf (flog, "%s; ", env);             // log de l'address IP distante
   env = getenv ("HTTP_USER_AGENT");
   if (strlen (env) > 8) *(env + 8) = '\0'; // tronque a 10 caracteres
   fprintf (flog, "%s; ", env);              // log du user agent distant

   // Lecture de la chaine de caractere representant le jeu via la methode post

   env = getenv ("QUERY_STRING");             // Les variables GET
   if (env == NULL) return;

   if ((str = strstr (env, "fen=")) != NULL)
      sscanf (str, "fen=%[bwprnbqkPRNBQK1-8+-/]", getInfo.fenString);
   if ((str = strstr (env, "level=")) != NULL)
      sscanf (str, "level=%d", &getInfo.level);
   if ((str = strstr (env, "reqType=")) != NULL)
      sscanf (str, "reqType=%d", &getInfo.reqType);

   if (getInfo.reqType != 0) {
       fenToGame (getInfo.fenString, sq64, &getInfo.activeColor);
       computerPlay (sq64);
       sendGame(sq64, info, getInfo.reqType);
       fprintf (flog, "%2d; %s; %s; %d", getInfo.level, getInfo.fenString, info.computerPlay, info.note);
   }
   else sendGame (sq64, info, getInfo.reqType);
   fprintf (flog, "\n");
}

int main (int argc, char *argv[]) { /* */
   /* lit la ligne de commande */
   /* -i [FENGame] [profondeur] : CLI avec sortie JSON */
   /* -r [FENGame] [profondeur] : CLI avec sortie raw */
   /* -t : test unitaire */
   /* -h : help */
   /* autrement CGI */
   char fen [MAXLENGTH];
   // preparation du fichier log 
   flog = fopen (F_LOG, "a");
   info.nClock = clock ();
   
   srand (time (NULL)); // initialise le generateur aleatoire
   if (argc >= 2) {
      if ((strcmp (argv[1], "-i") == 0) || (strcmp (argv[1], "-r") == 0)) {
         if (argc > 2) fenToGame (argv [2], sq64, &getInfo.activeColor);
         if (argc > 3) getInfo.level = atoi (argv [3]);
         printf ("fen: %s, level: %d\n", argv [2], getInfo.level);
         computerPlay (sq64);
         if (strcmp (argv[1], "-i") == 0) { //JSON
            sendGame (sq64, info, getInfo.reqType);
         }
         else { //raw
            gameToFen (sq64, fen, info.gamerColor, '+', true); 
            printf ("%s\n", fen);
         }
         fclose (flog);
         exit (1);
      }
      if (strcmp (argv[1], "-t") == 0) {
         // tests
         fenToGame (argv [2], sq64, &getInfo.activeColor);
         printGame (sq64, 0);
         info.gamerColor = (getInfo.activeColor == 'b')? -1 : 1;
         nextL = buildList(sq64, -info.gamerColor, list);         
         printf ("Nombre de possibilites : %d\n", nextL);
         for (int i = 0; i < nextL; i++) printGame (list [i], 0);
         exit (1);
      }
      if (strcmp (argv[1], "-h") == 0) {
         printf ("%s\n", HELP);
         exit (1);
      }
   }
   
   cgi ();
   fclose (flog);
}

