/*   Pour produire la doc sur les fonctions : grep "\/\*" chess.c | sed 's/^\([a-zA-Z]\)/\n\1/' */
/*   Jeu d'echec */
/*   ./chess.cgi -i [FENGame] [profondeur] : CLI avec sortie JSON */
/*   ./chess.cgi -r [FENGame] [profondeur] : CLI avec sortie raw */
/*   ./chess.cgi -t : test unitaire */
/*   ./chess.cgi -p [FENGame] [profondeur] : play mode CLI */
/*   ./chess.cgi -e [FENGame] [profondeur] : endurance. joue contre lui même */
/*   ./chess.cgi -h : help */
/*   autrement CGI */
/*   script CGI gérant une API restful (GET) avec les réponses au format JSON */
/*   fichiers associes : chess.log, chessB.fen, chessW.fen, chessUtil.c, syzygy.c, tbprobes.c tbcore.c et .h associes  */
/*   Structures de donnees essentielles */
/*     - jeu represente dans un table a 2 dimensions : TGAME sq64 */
/*     - liste de jeux qui est un tableau de jeux :  TLIST list */
/*     - nextL est un entier pointant sur le prochain jeux a inserer dans la liste */
/*   Noirs : 1 (Minuscules) */
/*   Blancs : -1 (Majuscules) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdbool.h>

#include "chessUtil.h"
#include "syzygy.h"

FILE *flog;
bool test = false;

struct sGetInfo {                  // description de la requete emise par le client
   char fenString [MAXLENGTH];     // le jeu
   int reqType;                    // le type de requete : 0 1 ou 2
   int level;                      // la profondeur de la recherche souhaitee
} getInfo = {"", 2, 3};

// valorisation des pieces dans l'ordre  VOID PAWN KNIGHT BISHOP ROOK QUEEN KING CASTLEKING
// tour = fou + 2 pions, dame >= fou + tour + pion
// pion a la valeur 8 suur les cotes, 9 sur les  colonnes centrales et plus importante quand avances 
// Le roi qui a roque a le code 7, le roi normal code 6
// Voir fonction d'evaluation
int val [] = {0, 1000, 3000, 3000, 5000, 9000, 0, 100};
int tEval [MAXTHREADS];

TGAME sq64 = {
   {-ROOK, -KNIGHT, -BISHOP, -QUEEN, -KING, -BISHOP, -KNIGHT, -ROOK},
   {-PAWN, -PAWN, -PAWN, -PAWN, -PAWN, -PAWN, -PAWN, -PAWN},
   {0, 0, 0, 0, 0, 0, 0, 0},
   {0, 0, 0, 0, 0, 0, 0, 0},
   {0, 0, 0, 0, 0, 0, 0, 0},
   {0, 0, 0, 0, 0, 0, 0, 0},
   {PAWN, PAWN, PAWN, PAWN, PAWN, PAWN, PAWN, PAWN},
   {ROOK, KNIGHT, BISHOP, QUEEN, KING, BISHOP, KNIGHT, ROOK}
};
TLIST list;
int nextL; // nombre total utilisé dans la pile
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int fMaxDepth (int lev, struct sinfo info) { /* */
   /* renvoie la profondeur du jeu en fonction du niveau choisi et */
   /* de l'etat du jeu */
   const struct {      
      int v;
      int inc;
   } val [] = {{200, 3}, {400, 2},  {800, 1}};

   int prod = info.nValidComputerPos * info.nValidGamerPos;
   for (int i = 0; i < NDEPTH; i++)
      if (prod < val [i].v) return val [i].inc + lev;
   return lev;
}

bool LCBlackKingInCheck (TGAME sq64, register int l, register int c) { /* */
   /* vrai si le roi Noir situe case l, c est echec au roi */
   register int w, k;
   //pthread_mutex_lock (&mutex);
   //info.nLCKingInCheckCall += 1;
   //pthread_mutex_unlock (&mutex);

   // roi adverse  menace.  Matche -KING et -CASTLEKING
   if (l < 7 && (-sq64 [l+1][c] >= KING)) return true;
   if (l > 0 && (-sq64 [l-1][c] >= KING)) return true;
   if (c < 7 && (-sq64 [l][c+1] >= KING)) return true;
   if (c > 0 && (-sq64 [l][c-1] >= KING)) return true;
   if (l < 7 && c < 7 && (-sq64 [l+1][c+1] >= KING)) return true;
   if (l < 7 && c > 0 && (-sq64 [l+1][c-1] >= KING)) return true;
   if (l > 0 && c < 7 && (-sq64 [l-1][c+1] >= KING || -sq64 [l-1][c+1] == PAWN)) return true;
   if (l > 0 && c > 0 && (-sq64 [l-1][c-1] >= KING || -sq64 [l-1][c-1] == PAWN)) return true;

   // cavalier menace
   if (l < 7 && c < 6 && (-sq64 [l+1][c+2] == KNIGHT)) return true;
   if (l < 7 && c > 1 && (-sq64 [l+1][c-2] == KNIGHT)) return true;
   if (l < 6 && c < 7 && (-sq64 [l+2][c+1] == KNIGHT)) return true;
   if (l < 6 && c > 0 && (-sq64 [l+2][c-1] == KNIGHT)) return true;
   if (l > 0 && c < 6 && (-sq64 [l-1][c+2] == KNIGHT)) return true;
   if (l > 0 && c > 1 && (-sq64 [l-1][c-2] == KNIGHT)) return true;
   if (l > 1 && c < 7 && (-sq64 [l-2][c+1] == KNIGHT)) return true;
   if (l > 1 && c > 0 && (-sq64 [l-2][c-1] == KNIGHT)) return true;

   // tour ou reine menace
   for (k = l+1; k < N; k++) {
      w = -sq64 [k][c];
      if (w == ROOK || w == QUEEN) return true;
      if (w != 0) break;
   }
   for (k = l-1; k >= 0; k--) {
      w = -sq64 [k][c];
      if (w == ROOK || w == QUEEN) return true;
      if (w != 0) break;
   }
   for (k = c+1; k < N; k++) {
      w = -sq64 [l][k];
      if (w == ROOK || w == QUEEN) return true;
      if (w != 0) break;
   }
   for (k = c-1; k >= 0; k--) {
      w = -sq64 [l][k];
      if (w == ROOK || w == QUEEN) return true;
      if (w != 0) break;
   }

   // fou ou reine menace
   for (k = 0; k < MIN (7-l, 7-c); k++) { // vers haut droit
      w = -sq64 [l+k+1][c+k+1];
      if ((w == BISHOP || w == QUEEN) != 0) return true;
      if (w != 0) break;
   }
   for (k = 0; k < MIN (7-l, c); k++) {// vers haut gauche
      w = -sq64 [l+k+1][c-k-1];
      if ((w == BISHOP || w == QUEEN) != 0) return true;
      if (w != 0) break;
   }
   for (k = 0; k < MIN (l, 7-c); k++) { // vers bas droit
      w = -sq64 [l-k-1][c+k+1];
      if ((w == BISHOP || w == QUEEN) != 0) return true;
      if (w != 0) break;
   }
   for (k = 0; k < MIN (l, c); k++) { // vers bas gauche
      w = -sq64 [l-k-1] [c-k-1];
      if ((w == BISHOP || w == QUEEN) != 0) return true;
      if (w != 0) break;
   }
   return false;
}

bool LCWhiteKingInCheck (TGAME sq64, register int l, register int c) { /* */
   /* vrai si le roi Blanc situe case l, c est echec au roi */
   register int w, k;
   //pthread_mutex_lock (&mutex);
   //info.nLCKingInCheckCall += 1;
   //pthread_mutex_unlock (&mutex);

   // who : -1
   // roi adverse  menace. >= KING marche KING et CASTLELING
   if (l < 7 && (sq64 [l+1][c] >= KING)) return true;
   if (l > 0 && (sq64 [l-1][c] >= KING)) return true;
   if (c < 7 && (sq64 [l][c+1] >= KING)) return true;
   if (c > 0 && (sq64 [l][c-1] >= KING)) return true;
   if (l < 7 && c < 7 && (sq64 [l+1][c+1] >= KING || sq64 [l+1][c+1] == PAWN)) return true;
   if (l < 7 && c > 0 && (sq64 [l+1][c-1] >= KING || sq64 [l+1][c-1] == PAWN)) return true;
   if (l > 0 && c < 7 && (sq64 [l-1][c+1] >= KING)) return true;
   if (l > 0 && c > 0 && (sq64 [l-1][c-1] >= KING)) return true;

   // cavalier menace
   if (l < 7 && c < 6 && (sq64 [l+1][c+2] == KNIGHT)) return true;
   if (l < 7 && c > 1 && (sq64 [l+1][c-2] == KNIGHT)) return true;
   if (l < 6 && c < 7 && (sq64 [l+2][c+1] == KNIGHT)) return true;
   if (l < 6 && c > 0 && (sq64 [l+2][c-1] == KNIGHT)) return true;
   if (l > 0 && c < 6 && (sq64 [l-1][c+2] == KNIGHT)) return true;
   if (l > 0 && c > 1 && (sq64 [l-1][c-2] == KNIGHT)) return true;
   if (l > 1 && c < 7 && (sq64 [l-2][c+1] == KNIGHT)) return true;
   if (l > 1 && c > 0 && (sq64 [l-2][c-1] == KNIGHT)) return true;

   // tour ou reine menace
   for (k = l+1; k < N; k++) {
      w = sq64 [k][c];
      if (w == ROOK || w == QUEEN) return true;
      if (w != 0) break;
   }
   for (k = l-1; k >= 0; k--) {
      w = sq64 [k][c];
      if (w == ROOK || w == QUEEN) return true;
      if (w != 0) break;
   }
   for (k = c+1; k < N; k++) {
      w = sq64 [l][k];
      if (w == ROOK || w == QUEEN) return true;
      if (w != 0) break;
   }
   for (k = c-1; k >= 0; k--) {
      w = sq64 [l][k];
      if (w == ROOK || w == QUEEN) return true;
      if (w != 0) break;
   }

   // fou ou reine menace
   for (k = 0; k < MIN (7-l, 7-c); k++) { // vers haut droit
      w = sq64 [l+k+1][c+k+1];
      if (w == BISHOP || w == QUEEN) return true;
      if (w != 0) break;
   }
   for (k = 0; k < MIN (7-l, c); k++) {// vers haut gauche
      w = sq64 [l+k+1][c-k-1];
      if (w == BISHOP || w == QUEEN) return true;
      if (w != 0) break;
   }
   for (k = 0; k < MIN (l, 7-c); k++) { // vers bas droit
      w = sq64 [l-k-1][c+k+1];
      if (w == BISHOP || w == QUEEN) return true;
      if (w != 0) break;
   }
   for (k = 0; k < MIN (l, c); k++) { // vers bas gauche
      w = sq64 [l-k-1] [c-k-1];
      if (w == BISHOP || w == QUEEN) return true;
      if (w != 0) break;
   }
   return false;
}

bool LCkingInCheck (TGAME sq64, register int who, register int l, register int c) { /* */
   /* vrai si le roi situe case l, c est echec au roi */
   /* "who" est la couleur du roi qui est attaque */
   return (who == 1) ? LCBlackKingInCheck (sq64, l, c) : LCWhiteKingInCheck (sq64, l, c);
}

inline char *pushList (TGAME refJeu, TLIST list, int nListe, char *pl, int l1, int c1, int l2, int c2, int u) { /* */
   /* pousse le jeu refJeu dans la liste avec les modifications specifiees */
   memcpy (pl, refJeu, GAMESIZE);
   list [nListe][l2][c2] = u;
   list [nListe][l1][c1] = 0;
   return (pl + GAMESIZE); 
}

int buildListEnPassant (TGAME refJeu, register int who, char *epGamer, TLIST list, int nextL) { /* */
   /* apporte le complement de positions a buildList prenant en compte en Passant suggere par le joueur */
   char *pl = &list [nextL][0][0];
   int nListe = nextL;
   if (epGamer [0] == '-') return nListe;
   int lEp = epGamer [1] - '1';
   int cEp = epGamer [0] - 'a';
   if ((cEp > 0) && (refJeu [lEp+who][cEp-1] == who * PAWN) &&      // vers droite
      (refJeu[lEp][cEp] == 0) && (refJeu [lEp+who][cEp] == -who * PAWN)) {
      pl = pushList (refJeu, list, nListe++, pl, lEp+who, cEp-1, lEp, cEp, who * PAWN);
      list [nListe -1][lEp+who][cEp] = 0;
   }
   if ((cEp < N) && (refJeu [lEp+who][cEp+1] == who * PAWN) &&      // vers gauche
      (refJeu [lEp][cEp] == 0) && (refJeu [lEp+who][cEp] == -who * PAWN)) { 
      pl = pushList (refJeu, list, nListe++, pl, lEp+who, cEp+1, lEp, cEp, who * PAWN);
      list [nListe -1][lEp+who][cEp] = 0;
   }
   return nListe;
}

int buildList (TGAME refJeu, register int who, TLIST list) { /* */
   /* construit la liste des jeux possibles a partir de jeu. */
   /* 'who' joue */
   register int u, v, k, l, c;
   register int nListe = 0;
   char *pl = &list [0][0][0];
   char *pr = &refJeu [0][0];
   // info.nBuildListCall += 1;
   int base = (who == -1) ? 0 : 7;

   // Roque
   if (refJeu [base][4] == KING) {
      // roque gauche
      if (refJeu [base][0] == ROOK && 0 == refJeu [base][1] && 0 == refJeu [base][2] && 0 == refJeu [base][3] &&
         !LCkingInCheck (refJeu, who, base, 3) && !LCkingInCheck (refJeu, who, base, 4)) {
         // la case traversee par le roi et le roi ne sont pas echec au roi
         memcpy (pl, refJeu, GAMESIZE);
         list [nListe][base][0] = 0;
         list [nListe][base][2] = CASTLEKING;
         list [nListe][base][3] = ROOK;
         list [nListe++][base][4] = 0; 
         pl += GAMESIZE;
      }
      // Roque droit
      if (refJeu [base][7] == ROOK && 0 == refJeu [base][5] && 0  == refJeu [base][6] &&
         !LCkingInCheck (refJeu, who, base, 4) && !LCkingInCheck (refJeu, who, base, 5)) {
         // la case traversee par le roi et le roi ne sont pas echec au roi
         memcpy (pl, refJeu, GAMESIZE);
         list [nListe][base][4] = 0;
         list [nListe][base][5] = ROOK;
         list [nListe][base][6] = CASTLEKING;
         list [nListe++][base][7] = 0;
         pl += GAMESIZE;
      }
   }
   for (register int z = 0; z < GAMESIZE; z++) { 
      u = *(pr++); // u est la valeur courante dans refJeu
      v = who * u; // v = abs (u)
      if (v > 0) {
         l = LINE (z);
         c = COL (z);  
         switch (v) { // v est la valeur absolue car u < 0 ==> who < 0
         case PAWN:
         // deplacements du pion
            if (who == -1 && l == 1 && 0 == refJeu [l+1][c] && 0 == refJeu[l+2][c]) {  // coup initial saut de 2 cases 
               pl = pushList (refJeu, list, nListe++, pl, l, c, l+2, c, -PAWN);
            }
            if (who == 1 && l == 6 && 0 == refJeu [l-1][c] && 0 == refJeu [l-2][c]) {  // coup initial saut de 2 cases
               pl = pushList (refJeu, list, nListe++, pl, l, c, l-2, c, PAWN);
            }
            if (((l-who) >= 0) && ((l-who) < 8) && (0 == refJeu [l-who][c])) {         // normal
               memcpy (pl, refJeu, GAMESIZE);
               if (l-1 == 0 && who == 1)  list [nListe][l-1][c] = QUEEN;
               else if (l+1 == 7 && who == -1) list [nListe][l+1][c] = -QUEEN;
               else list [nListe][l-who][c] = PAWN * who;
               list [nListe][l][c] = 0;
               pl += GAMESIZE; nListe += 1;
            }
            // prise a droite
            if (c < 7 && (l-who) >=0 && (l-who) < N && refJeu [l-who][c+1]*who < 0) {  // signes opposes
               memcpy (pl, refJeu, GAMESIZE);
               if (l-1 == 0 && who == 1) list [nListe][l-1][c+1] = QUEEN;
               else if (l+1 == 7 && who == -1) list [nListe][l+1][c+1] = -QUEEN;
               else list [nListe][l-who][c+1] = PAWN * who;
               list [nListe][l][c] = 0;
               pl += GAMESIZE; nListe += 1;
            }
            // prise a gauche
            if (c > 0 && (l-who) >= 0 && (l-who) < N && refJeu [l-who][c-1]*who < 0) { // signes opposes
               memcpy (pl, refJeu, GAMESIZE);
               if (l-1 == 0 && who == 1) list [nListe][l-1][c-1] = QUEEN;
               else if (l+1 == 7 && who == -1) list [nListe][l+1][c-1] = -QUEEN;
               else list [nListe][l-who][c-1] = PAWN * who;
               list [nListe][l][c] = 0;
               pl += GAMESIZE; nListe += 1;
            }
            break;
         case KING: case CASTLEKING:
            if (c < 7 && (u * refJeu [l][c+1] <= 0)) {
               pl = pushList (refJeu, list, nListe++, pl, l, c, l, c+1, who * CASTLEKING);
            }
            if (c > 0 && (u * refJeu [l][c-1] <= 0)) {
               pl = pushList (refJeu, list, nListe++, pl, l, c, l, c-1, who * CASTLEKING);
            }
            if (l < 7 && (u * refJeu [l+1][c] <= 0)) {
               pl = pushList (refJeu, list, nListe++, pl, l, c, l+1, c, who * CASTLEKING);
            }
            if (l > 0 && (u * refJeu [l-1][c] <= 0)) {
               pl = pushList (refJeu, list, nListe++, pl, l, c, l-1, c, who * CASTLEKING);
            }
            if ((l < 7) && (c < 7) && (u * refJeu [l+1][c+1] <= 0)) {
               pl = pushList (refJeu, list, nListe++, pl, l, c, l+1, c+1, who * CASTLEKING);
            }
            if ((l < 7) && (c > 0) && (u * refJeu [l+1][c-1] <= 0)) {
               pl = pushList (refJeu, list, nListe++, pl, l, c, l+1, c-1, who * CASTLEKING);
            }
            if ((l > 0) && (c < 7) && (u * refJeu [l-1][c+1] <= 0)) {
               pl = pushList (refJeu, list, nListe++, pl, l, c, l-1, c+1, who * CASTLEKING);
            }
            if ((l > 0) && (c > 0) && (u * refJeu [l-1][c-1] <= 0)) {
               pl = pushList (refJeu, list, nListe++, pl, l, c, l-1, c-1, who * CASTLEKING);
            }
            break;

         case KNIGHT:
            if (l < 7 && c < 6 && (u * refJeu [l+1][c+2] <= 0)) {
               pl = pushList (refJeu, list, nListe++, pl, l, c, l+1, c+2, u);
            }
            if (l < 7 && c > 1 && (u * refJeu [l+1][c-2] <= 0)) {
               pl = pushList (refJeu, list, nListe++, pl, l, c, l+1, c-2, u);
            }
            if (l < 6 && c < 7 && (u * refJeu [l+2][c+1] <= 0)) {
               pl = pushList (refJeu, list, nListe++, pl, l, c, l+2, c+1, u);
            }
            if (l < 6 && c > 0 && (u * refJeu [l+2][c-1] <= 0)) {
               pl = pushList (refJeu, list, nListe++, pl, l, c, l+2, c-1, u);
            }
            if (l > 0 && c < 6 && (u * refJeu [l-1][c+2] <= 0)) {
               pl = pushList (refJeu, list, nListe++, pl, l, c, l-1, c+2, u);
            }
            if (l > 0  && c > 1 && (u * refJeu [l-1][c-2] <= 0)) {
               pl = pushList (refJeu, list, nListe++, pl, l, c, l-1, c-2, u);
            }
            if (l > 1 && c < 7 && (u * refJeu [l-2][c+1] <= 0)) {
               pl = pushList (refJeu, list, nListe++, pl, l, c, l-2, c+1, u);
            }
            if (l > 1 && c > 0 && (u * refJeu [l-2][c-1] <= 0)) {
               pl = pushList (refJeu, list, nListe++, pl, l, c, l-2, c-1, u);
            }
            break;

         case ROOK: case QUEEN:
            for (k = l+1; k < N; k++) { // en haut
               if (u * refJeu [k][c] <= 0) {
                  pl = pushList (refJeu, list, nListe++, pl, l, c, k, c, u);
                  if (refJeu [k][c] != 0)  break;
               }
               else break;
            }
            for (k = l-1; k >=0; k--) { // en bas
               if (u * refJeu [k][c] <= 0) {
                  pl = pushList (refJeu, list, nListe++, pl, l, c, k, c, u);
                  if (refJeu [k][c] != 0) break;
               }
               else break;
            }
            for (k = c+1; k < N; k++) {  // a droite
               if (u * refJeu [l][k] <= 0) {
                  pl = pushList (refJeu, list, nListe++, pl, l, c, l, k, u);
                  if (refJeu [l][k] != 0) break;
               }
               else break;
            }
            for (k = c-1; k >=0; k--)  { // a gauche
               if (u * refJeu [l][k] <= 0) {
                  pl = pushList (refJeu, list, nListe++, pl, l, c, l, k, u);
                  if (refJeu [l][k] != 0) break;
               }
               else break;
            }
            if (v == ROOK) break;
         // surtout pas de break pour la reine
         case BISHOP : // valable aussi pour QUEEN
            for (k = 0; k < MIN (7-l, 7-c); k++) { // vers haut droit
               if (u * refJeu [l+k+1][c+k+1] <=0) {
                  pl = pushList (refJeu, list, nListe++, pl, l, c, l+k+1, c+k+1, u);
                  if (refJeu [l+k+1][c+k+1] != 0) break;
               }
               else break;
            }
            for (k = 0; k < MIN (7-l, c); k++) { // vers haut gauche
               if (u * refJeu [l+k+1][c-k-1] <=0) {
                  pl = pushList (refJeu, list, nListe++, pl, l, c, l+k+1, c-k-1, u);
                  if (refJeu [l+k+1][c-k-1] != 0) break;
               }
               else break;
            }
            for (k = 0; k < MIN (l, 7-c); k++) { // vers bas droit
               if (u * refJeu [l-k-1][c+k+1] <=0) {
                  pl = pushList (refJeu, list, nListe++, pl, l, c, l-k-1, c+k+1, u);
                  if (refJeu [l-k-1][c+k+1] != 0) break;
               }
               else break;
            }
            for (k = 0; k < MIN (l, c); k++) { // vers bas gauche
               if (u * refJeu [l-k-1][c-k-1] <=0) {
                  pl = pushList (refJeu, list, nListe++, pl, l, c, l-k-1, c-k-1, u);
                  if (refJeu [l-k-1][c-k-1] != 0) break;
               }
               else break;
            }
            break;
         default:;
         } //fin du switch
      }    // fin du if
   }       // fin du for sur z
   return nListe;
}

bool fKingInCheck (TGAME sq64, register int who) { /* */
   /* retourne vrai si le roi "who" est en echec */
   for (register int l = 0; l < N; l++)
      for (register int c = 0; c < N; c++) 
         if ((who * sq64 [l][c]) >= KING) { // match KING et CASTLEKING
            if (LCkingInCheck (sq64, who, l, c)) return true;
            else return false;
	 }
   return false;
}

bool kingCannotMove (TGAME sq64, register int who) { /* */
   /* vrai si le roi du joueur "who" ne peut plus bouger sans se mettre echec au roi */
   /* "who" est la couleur du roi qui est attaque */
   /* on essaye tous les jeux possibles. Si dans tous les cas on est echec au roi */
   /* c'est perdu. Noter que si le roi a le trait et qu'il n'est pas echec au roi il est Pat */
   /* si le roi est echec au roi il est mat */
   TLIST list;
   register int maxList = buildList (sq64, who, list);
   for (register int k = 0; k < maxList; k++) {
      if (! fKingInCheck (list [k], who)) return false;
   }
   return true;
}

int evaluation (TGAME sq64, int who) { /* */
   /* fonction d'evaluation retournant MATE si Ordinateur gagne, */
   /* -MAT si joueur gagne, 0 si nul,... */
   int l, c, v, eval;
   char *p64 = &sq64 [0][0];
   int lwho, cwho, ladverse, cadverse, nBishopPlus, nBishopMinus;
   bool kingInCheck;
   lwho = cwho = ladverse = cadverse = 0;
   eval = 0;
   nBishopPlus = nBishopMinus = 0;
   // info.nEvalCall += 1;
   for (register int z = 0; z < GAMESIZE; z++) {
        // eval des pieces
      if ((v = (*p64++)) == 0) continue;
      // v est la valeur courante
      l = LINE (z);
      c = COL (z);
      eval += ((v > 0) ? val [v] : -val [-v]);
       
      switch (v) {
      case KING : case CASTLEKING :
         if (who == 1) { lwho = l; cwho = c; }
         else  { ladverse = l; cadverse = c; }
         break;
      case -KING: case -CASTLEKING :
         if (who == 1) { ladverse = l; cadverse = c; }
         else  {lwho = l; cwho = c;}
         break;
      case KNIGHT: // on privilégie cavaliers au centre
         if (c >= 2 && c <= 5 && l >= 2 && l <= 5) eval += BONUSCENTER;
         break;
      case -KNIGHT:
         if (c >= 2 && c <= 5 && l >= 2 && l <= 5) eval -= BONUSCENTER;
         break;
      case BISHOP: // on attribue un bonus si deux fous de la meme couleur
         nBishopPlus += 1;
         break;
      case -BISHOP:
         nBishopMinus += 1;
         break;
      case ROOK: // bonus si tour mobile
         if (l == 7) {
           if (((c == 0) && sq64 [6][0] == 0) || ((c == 7) && sq64 [6][7] == 0)) 
              eval += BONUSMOVEROOK;   
         }
         break;
      case -ROOK:
         if (l == 0) {
           if (((c == 0) && sq64 [1][0] == 0) || ((c == 7) && sq64 [1][7] == 0)) 
            eval -= BONUSMOVEROOK;   
         }
         break;
      case PAWN: // Si noir, plus on est pres de la ligne 0, mieuxx c'est
         eval += ((N - 1) - l) * BONUSPAWNAHEAD;  
         break;
      case -PAWN: // Si blanc, le contraire
         eval -= l * BONUSPAWNAHEAD;
         break;
      default:;
      }
   }
   // printf ("eval 1 : %d\n", eval);
   // printf ("lwho: %d, cwho : %d, ladverse : %d, cadverse : %d\n", lwho, cwho, ladverse, cadverse);
   if (nBishopPlus >= 2) eval += BONUSBISHOP;
   if (nBishopMinus >= 2) eval -= BONUSBISHOP;
   if (LCkingInCheck (sq64, who, lwho, cwho)) return -who * MATE; // who ne peut pas jouer et se mettre en echec
   kingInCheck = LCkingInCheck (sq64, -who, ladverse, cadverse);
   if (kingCannotMove(sq64, -who)) { 
      if (kingInCheck) return who * MATE;
      else return 0; // Pat
   }
   if (kingInCheck) eval += who * KINGINCHECKEVAL;
   return eval;
}

int alphaBeta (TGAME sq64, int who, int p, int refAlpha, int refBeta) { /* */
   /* le coeur du programme */
   TLIST list;
   int maxList = 0;
   int k, note;
   int val;
   int alpha = refAlpha;
   int beta = refBeta;
   note = evaluation (sq64, who);
   if (info.calculatedMaxDepth < p) info.calculatedMaxDepth = p;

   // conditions de fin de jeu
   if (p >= info.maxDepth) return note;
   if (note == MATE) return note-p;      // -p pour favoriser le choix avec faible profondeur
   if (note == -MATE) return note+p;     // +p idem
   // pire des notes a ameliorer
   if (who == 1) {
      val = MATE;
      maxList = buildList (sq64, -1, list);
      for (k = 0; k < maxList; k++) {
         note = alphaBeta (list [k], -1, p+1, alpha, beta);
         if (note < val) val = note;    // val = minimum...
         if (alpha > val) return val;
         if (val < beta) beta = val;    // beta = MIN (beta, val);
      }
   }
   else {
   // recherche du maximum
      val = -MATE;
      maxList = buildList (sq64, 1, list);
      for (k = 0; k < maxList; k++) {
         note = alphaBeta (list [k], 1, p+1, alpha, beta);
         if (note > val) val = note;    // val = maximum...
         if (beta < val) return val;
         if (val > alpha) alpha = val;  // alpha = max (alpha, val);
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

void updateInfo (TGAME sq64) { /* */
   /* met a jour l'objet info a partir de l'objet jeu */
   int l, c, v;
   info.note = evaluation(sq64, info.gamerColor);
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

   if (info.computerKingState == ISMATE) 
      info.score = (info.gamerColor == -1) ? WHITEWIN : BLACKWIN;
   else if (info.gamerKingState == ISMATE)
      info.score = (info.gamerColor == -1) ? BLACKWIN: WHITEWIN;
   else if (info.computerKingState == ISPAT || info.gamerKingState == ISPAT || info.cpt50 > 50)
      info.score = DRAW;
}

int find (TGAME sq64, TGAME bestSq64, int *bestNote, int color) { /* */
   /* ordinateur joue renvoie le meilleur jeu possible et le nombre de jeux possibles */
   TGAME localSq64;
   int i, random;
   long k;
   pthread_t tThread [MAXTHREADS];
   int possible [MAXSIZELIST]; // tableau contenant les indice de jeux ayant la meilleure note
   char fen [MAXBUFFER] = "";
   
   *bestNote = 0; 
   nextL = buildList(sq64, color, list);
   nextL = buildListEnPassant (sq64, color, info.epGamer, list, nextL);
   strcpy (info.comment, "");

   
   if (nextL == 0) return 0;

   // Hasard Total
   if (getInfo.level == -1) {
      k = rand () % nextL; // renvoie un entier >= 0 et strictement inferieur a nextL
      memcpy (bestSq64, list [k], GAMESIZE);
      return nextL;
   }
   
   memcpy (localSq64, sq64, GAMESIZE);
   gameToFen (localSq64, fen, color, ' ', false, info.epComputer, 0, 0);

   // recherche de fin de partie voir https://syzygy-tables.info/
   if ((info.nGamerPieces + info.nComputerPieces) <= MAXPIECESSYZYGY) {
      sprintf (fen, "%s - - %d %d", fen, info.cpt50, info.nb); // pour regle des 50 coups
      if (syzygyRR (PATHTABLE, fen, &info.wdl, info.move, info.endName)) {
         moveGame (localSq64, color, info.move);
         memcpy (bestSq64, localSq64, GAMESIZE);
	      return nextL;
      }
   }
     
   // ouvertures
   if (info.nb < MAXNBOPENINGS) {
      if (openingAll (OPENINGDIR, (color == 1) ? ".b.fen": ".w.fen", fen, info.comment, info.move)) {
         moveGame (localSq64, color, info.move);
         memcpy (bestSq64, localSq64, GAMESIZE);
         return nextL;
      }
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
   /*
   for (k = 0; k < nextL; k++)
      tEval [k] = alphaBeta (list [k], -info.gamerColor, 0, -MATE, MATE);
   */
   if (test) {
      printGame (sq64, 0);
      printf ("===============================\n");
      for (int k = 0; k < nextL; k++) printGame (list [k], tEval [k]);
   }
   // tEval contient les évaluations de toutes les possibilites
   // recherche de la meilleure note
   for (k = 0; k < nextL; k++) {
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
   if (test) printf ("Nb de jeu : %d ayant la meilleure note : %d \n", i, *bestNote); 
   info.nBestNote = i;
   random  = rand () % i; // renvoie un entier >=  0 et strictement inferieur a i
   k = possible [random]; // indice du jeu choisi au hasard
   // k = possible [0];      // deterministe A ENLEVER SI RANDOM PREFERE
   memcpy (bestSq64, list [k], GAMESIZE);

   return nextL;
}

void computerPlay (TGAME sq64, int color) { /* */
   /* prepare le lancement de la recherche avec find */
   struct timeval tRef;
   TGAME bestSq64;
   updateInfo (sq64);
   if (info.gamerKingState == ISINCHECK)
      info.gamerKingState = UNVALIDINCHECK; // le joueur n'a pas le droit d'etre en echec
   
   if (info.computerKingState == ISMATE || info.computerKingState == ISPAT ||
      info.computerKingState == NOEXIST || info.gamerKingState == UNVALIDINCHECK)
      return;

   info.maxDepth = fMaxDepth (getInfo.level, info);
   gettimeofday (&tRef, NULL);
   info.computeTime = tRef.tv_sec * MILLION + tRef.tv_usec;
   info.nClock = clock ();

   // lancement de la recherche par l'ordi
   if ((info.nValidComputerPos = find (sq64, bestSq64, &info.evaluation, color)) != 0) {
      difference (sq64, bestSq64, color, &info.lastCapturedByComputer, info.computerPlayC, info.computerPlayA, 
         info.epGamer, info.epComputer);
      if (color == 1) info.nb += 1;
      if (abs (info.computerPlayC [0] == 'P') ||  info.computerPlayC [3] == 'x') // si un pion bouge ou si prise
         info.cpt50 = 0;
      else info.cpt50 += 1;
      memcpy(sq64, bestSq64, GAMESIZE);
   }
   info.nClock = clock () - info.nClock;
   gettimeofday (&tRef, NULL);
   info.computeTime = tRef.tv_sec * MILLION + tRef.tv_usec - info.computeTime;
   updateInfo(sq64);
   if (info.computerKingState == ISINCHECK) // pas le droit d'etre ehec apres avoir joue
      info.computerKingState = UNVALIDINCHECK;
   return;
}

void cgi () { /* */
   /* MODE CGI
   */
   char fen [MAXLENGTH];
   char *str;
   char *env;
   char temp [MAXBUFFER];
   str = temp;
   char buffer [80] = "";
   srand (time (NULL)); // initialise le generateur aleatoire
   
   // log date heure et adresse IP du joueur
   time_t now = time (NULL); // pour .log
   struct tm *timeNow = localtime (&now);
   strftime (buffer, 80, "%F; %T", timeNow);
   fprintf (flog, "%s; ", buffer);           // log de la date et du temps
   env = buffer;
   env = getenv ("REMOTE_ADDR");
   fprintf (flog, "%s; ", env);              // log de l'address IP distante
   env = getenv ("HTTP_USER_AGENT");
   if (strlen (env) > 8) *(env + 8) = '\0';  // tronque a 10 caracteres
   fprintf (flog, "%s; ", env);              // log du user agent distant

   // Lecture de la chaine de caractere representant le jeu via la methode post

   if ((env = getenv ("QUERY_STRING")) == NULL) return;  // Les variables GET

   if ((str = strstr (env, "fen=")) != NULL)
      sscanf (str, "fen=%[a-zA-Z0-9+-/]", getInfo.fenString);
   if ((str = strstr (env, "level=")) != NULL)
      sscanf (str, "level=%d", &getInfo.level);
   if ((str = strstr (env, "reqType=")) != NULL)
      sscanf (str, "reqType=%d", &getInfo.reqType);
 
   if (getInfo.reqType != 0) {
       info.gamerColor = -fenToGame (getInfo.fenString, sq64, info.epGamer, &info.cpt50, &info.nb);
       computerPlay (sq64, -info.gamerColor);
       gameToFen (sq64, fen, info.gamerColor, '+', true, info.epComputer, info.cpt50, info.nb);
       sendGame (fen, info, getInfo.reqType);
       fprintf (flog, "%2d; %s; %s; %d", getInfo.level, getInfo.fenString, info.computerPlayC, info.note);
   }
   else sendGame ("", info, getInfo.reqType);
   fprintf (flog, "\n");
}

int main (int argc, char *argv[]) { /* */
   /* lit la ligne de commande */
   /* -i [FENGame] [profondeur] : CLI avec sortie JSON */
   /* -r [FENGame] [profondeur] : CLI avec sortie raw */
   /* -p [FENGame] [profondeur] : play mode CLI */
   /* -t : test unitaire */
   /* -h : help */
   /* autrement CGI */
   char fen [MAXLENGTH];
   char strMove [15];
   int color;
   TGAME oldSq64;
   // preparation du fichier log 
   flog = fopen (F_LOG, "a");
   info.wdl = 9;           // valeur inateignable montrant que syzygy n'a pas ete appelee
   srand (time (NULL));    // initialise le generateur aleatoire
   info.gamerColor = 1;

   // si pas de parametres on va au cgi (fin se programme)
   if (argc >= 2 && argv [1][0] == '-') { // si il y a des parametres. On choidi un test
      if (argc > 2) info.gamerColor = -fenToGame (argv [2], sq64, info.epGamer, &info.cpt50, &info.nb);
      if (argc > 3) getInfo.level = atoi (argv [3]);
      switch (argv [1][1]) {
      case 'i': case 'I' :
         test = (argv [1][1] == 'I');
         printf ("fen: %s, level: %d\n", 
            gameToFen (sq64, fen, -info.gamerColor, '+', true, info.epComputer, info.cpt50, info.nb), getInfo.level);
         computerPlay (sq64, -info.gamerColor);
         gameToFen (sq64, fen, info.gamerColor, '+', true, info.epComputer, info.cpt50, info.nb); 
         sendGame (fen, info, getInfo.reqType);
         break;
      case 'r':
         computerPlay (sq64, -info.gamerColor);
         printf ("--------resultat--------------\n");
         printGame (sq64, evaluation (sq64, -info.gamerColor));
         gameToFen (sq64, fen, info.gamerColor, '+', true, info.epComputer, info.cpt50, info.nb);
         printf ("clockTime: %ld, time: %ld, note: %d, eval: %d, computerStatus: %d, playerStatus: %d\n", 
                 info.nClock, info.computeTime, info.note, info.evaluation, info.computerKingState, info.gamerKingState); 
         printf ("comment: %s%s\n", info.comment, info.endName);
         printf ("fen: %s\n", fen);
         printf ("move: %s %s %c\n", info.computerPlayC, (info.lastCapturedByComputer != '\0') ? "taken:": "", 
                 info.lastCapturedByComputer);
         break;
      case 'd': // test difference
         fenToGame ("2k5/8/8/8/4Pp2/8/8/2K5+b+-+e3+50+1", oldSq64, info.epGamer, &info.cpt50, &info.nb);
         fenToGame ("2k5/8/8/8/4Pp2/8/8/2K5+b+-+e3+50+1", sq64, info.epGamer, &info.cpt50, &info.nb);
         // fenToGame ("2k5/8/8/8/8/4p3/8/2K5+b+-+e3+50+1", sq64, info.epGamer, &info.cpt50, &info.nb);
         printGame (oldSq64, 0);
         printGame (sq64, 0);
         difference (oldSq64, sq64, 1, &info.lastCapturedByComputer, info.computerPlayC, 
            info.computerPlayA, info.epGamer, info.epComputer);
         printf ("last Captured: %c\n", info.lastCapturedByComputer);
         printf ("Dep Complet: %s\n", info.computerPlayC);
         printf ("Dep Abrege: %s\n", info.computerPlayA);
         printf ("ep Gamer: %s\n", info.epGamer);
         break;
      case 'f': //performance
         info.nClock = clock ();
         for (int i = 0; i < getInfo.level * MILLION; i++)
            LCkingInCheck (sq64, 1, 7, 4);
         printf ("LCKingInCheck. clock: %lf\n", (double) (clock () - info.nClock)/CLOCKS_PER_SEC);
         
         info.nClock = clock ();
         for (int i = 0; i < getInfo.level * MILLION; i++)
            nextL = buildList(sq64, -info.gamerColor, list);         
         printf ("buildList. clock: %lf\n", (double) (clock () - info.nClock)/CLOCKS_PER_SEC);
         
         info.nClock = clock ();
         for (int i = 0; i < getInfo.level * MILLION; i++)
            kingCannotMove (sq64, -1);
         printf ("kingCannotMove. clock: %lf\n", (double) (clock () - info.nClock)/CLOCKS_PER_SEC);
         
         info.nClock = clock ();
         for (int i = 0; i < getInfo.level * MILLION; i++)
            evaluation (sq64, 1);
         printf ("evaluation. clock: %lf\n", (double) (clock () - info.nClock)/CLOCKS_PER_SEC);
         break;
      case 'e': // endurance
         while (info.score == ONGOING) {
            info.lastCapturedByComputer = '\0';
            printGame (sq64, evaluation (sq64, -info.gamerColor));
            computerPlay (sq64, -info.gamerColor);
            printf ("clockTime: %ld, time: %ld, note: %d, eval: %d, computerStatus: %d, playerStatus: %d\n", 
                 info.nClock, info.computeTime, info.note, info.evaluation, info.computerKingState, info.gamerKingState); 
            printf ("comment: %s%s\n", info.comment, info.endName);
            printf ("move: %s\n", info.computerPlayC);
            info.gamerColor *= -1;
         }
         printf ("final\n");
         printGame (sq64,  evaluation (sq64, -info.gamerColor));
         break;      
      case 't':
         // tests
         printGame (sq64, evaluation (sq64, -info.gamerColor));
         printf ("==============================================================\n");
         nextL = buildList(sq64, -info.gamerColor, list);         
         nextL = buildListEnPassant (sq64, -info.gamerColor, info.epGamer, list, nextL);
         printf ("Nombre de possibilites : %d\n", nextL);
         for (int i = 0; i < nextL; i++) printGame (list [i], evaluation (list [i], -info.gamerColor));
         break;
      case 'p':
         info.gamerColor = -1;
         printf ("whe have the: %s\n", (info.gamerColor == -1) ? "Whites" : "Blacks");
         printGame (sq64, evaluation (sq64, -info.gamerColor));
         bool player = (info.gamerColor == -1); 
         while (info.score == ONGOING) {
            if (player) { // joueur joue
               printf ("gamer move: ");
               while (scanf ("%s", strMove) != 1);
               moveGame (sq64, info.gamerColor, strMove);
               printGame (sq64, evaluation (sq64, info.gamerColor));
            }
            else { // ordinateur joue
               memcpy (oldSq64, sq64, GAMESIZE);
               computerPlay (sq64, -info.gamerColor);
               printGame (sq64, evaluation (sq64, -info.gamerColor));
               printf ("comment: %s%s\n", info.comment, info.endName);
               printf ("computer move: %s %c\n", info.computerPlayC, info.lastCapturedByComputer);
            }
            player = !player;
         }
         break;
      case 'z':
         for (int i = 0; i < strlen (argv [2]); i++) if (argv [2][i] == '+') argv [2][i] = ' ';
         printf ("fen : %s\n", argv [2]);
         syzygyRR (PATHTABLE, argv [2], &info.wdl, info.move, info.endName);
         printf ("%s\n", info.endName);
         break;
      case 'l':
         color = 1;
          if (openingAll (OPENINGDIR, (color == 1) ? "b.fen": "w.fen", fen, info.comment, info.move)) {
            printf ("comment: %s, move; %s\n", info.comment, info.move);
         }
         else printf ("KO\n");
         break;
      case 'm': 
         printf ("%d\n", fMaxDepth (atoi (argv [2]), info));
         break;
      case 'h':
         printf ("%s\n", HELP);
         break;
      case 'v':
         printf ("Eval: %d\n", evaluation (sq64, -info.gamerColor));
         break;
         
      default: break;
      }
   }
   else cgi (); // la voie normale : pas de parametre => cgi.
   fclose (flog);
   exit (EXIT_SUCCESS);
}
