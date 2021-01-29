/*   Pour produire la doc sur les fonctions : grep "\/\*" chess.c | sed 's/^\([a-zA-Z]\)/\n\1/' */
/*   Jeu d'echec */
/*   ./chess.cgi -q |-v [FENGame] [profondeur] : CLI avec sortie JSON q)uiet v)erbose */
/*   ./chess.cgi -f : test performance */
/*   ./chess.cgi -t [FENGame] : test unitaire */
/*   ./chess.cgi -p [FENGame] [profondeur] : play mode CLI */
/*   ./chess.cgi -h : help */
/*   sans parametre ni option :  CGI gérant une API restful (GET) avec les réponses au format JSON */
/*   fichiers associes : chess.log, chessB.fen, chessW.fen, chessUtil.c, syzygy.c, tbprobes.c tbcore.c et .h associes  */
/*   Structures de donnees essentielles */
/*     - jeu represente dans un table a 2 dimensions : TGAME sq64 */
/*     - liste de jeux qui est un tableau de jeux :  TLIST list */
/*     - nextL est un entier pointant sur le prochain jeux a inserer dans la liste */
/*   Noirs : positifs  (Minuscules) */
/*   Blancs : negatifs  (Majuscules) */

#define MASQMAXTRANSTABLE 0x1fffffff            // 29 bits a 1 : 2 puissance 29-1 > 500  millions
//#define MASQMAXTRANSTABLE 0x0ffffff          // 24 bits a 1 : 2 puisssance 24 - 1 
#define MAXTRANSTABLE (MASQMAXTRANSTABLE + 1)
#define INITPROF 127 // pour table transpo
#define WHO(x)    ((x==-1)?0:0xffffffff)        //
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdbool.h>
#include <ctype.h>
#include "chessUtil.h"
#include "syzygy.h"

// valorisation des pieces dans l'ordre PAWN KNIGHT BISHOP ROOK QUEEN KING CASTLEKING
// Le roi qui a deja roque a le code 7, le roi normal a le code 6
// tour = fou + 2 pions, dame >= fou + tour + pion
// le roi n'a pas de valeur. Le roi qui a roque a un bonus
// Voir fonction d'evaluation
const int val [] = {0, 100, 300, 300, 500, 900, 0, BONUSCASTLE};

FILE *flog;

struct {                           // description de la requete emise par le client
   char fenString [MAXLENGTH];     // le jeu
   int reqType;                    // le type de requete : 0 1 ou 2
   int level;                      // la profondeur de la recherche souhaitee
   bool alea;                      // vrai si on prend un jeu de facon aleatoire quand plusieurs solutions
   bool trans;                     // vrai si on utilise les tables de transpo
   bool multi;                     // vrai si multithread
} getInfo = {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR+w+KQkq", 2, 4, true, true, true}; // par defaut

TGAME sq64;
TLIST list;
int nextL; // nombre total utilisé dans la file

typedef struct  {                  // tables de transposition
   int16_t eval;                   // derniere eval
   int8_t p;                       // profondeur
   int8_t used;                    // boolen sur 8 bits. Utilise
   int32_t check;                  // pour verid collisions
} StrTa;
StrTa *trTa = NULL;                // Ce pointeur va servir de tableau après l'appel du malloc

uint64_t ZobristTable[8][8][14];   // 14 combinaisons avec RoiRoque

inline bool sameParity (int a, int b) { /* */
   /* retource vrai si a et b ont la meme parite */
   return (a & 1) == (b & 1);
}

uint64_t rand64 () { /* */
   /* retourne un nb aleatoire sur 64 bits */
   return ((((uint64_t) rand ()) << 34) ^ (((uint64_t) rand ()) << 26) ^ (((uint64_t) rand ()) << 18) ^ (rand ()));
}

void initTable() { /* */
   /* Initializes the table Zobrist*/
   for (int l = 0; l < N; l++)
      for (int c = 0; c < N; c++)
         for (int k = 0; k < 14; k++) // 14 avec Roque
            ZobristTable[l][c][k] = rand64 ();
}

uint64_t computeHash (TGAME sq64, int who) { /* */
   /* Computes the hashvalue of a given game. Zobrist */
   int v;     // valeurs de -7 a 7. Case vide = 0. Pieces neg : blanches. Pos : noires.
   int piece; // valeurs de piece de 0 a 13. Case vide non representee
   uint64_t h = 0;
   for (int c = 0; c < N; c++)  {
      for (int l = 0; l < 8; l++) {
         if ((v = sq64[l][c]) != 0) { // Case vide non prise en compte
            piece = (v > 0) ? v - 1 : (-v + 6); // 0=pion noir, 13=roiroque blanc
            h ^= ZobristTable[l][c][piece];
          }
       }
    }
    // h est la valeur de Zobrist sur 64 bits. Que l'on va réduire à 32 bits.
    return (h ^ WHO (who)); 
}
  
int fMaxDepth (int lev, int nGamerPos, int nComputerPos) { /* */
   /* renvoie la profondeur du jeu en fonction du niveau choisi et */
   /* de l'etat du jeu */
   const struct { int v; int inc;
   } val [] = {{12, 7}, {25, 6}, {50, 5}, {100, 4}, {200, 3}, {400, 2},  {600, 1}};

   int prod = nComputerPos * nGamerPos;
   for (int i = 0; i < 7; i++)
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
      if ((w = -sq64 [k][c]) == ROOK || w == QUEEN) return true;
      if (w) break;
   }
   for (k = l-1; k >= 0; k--) {
      if ((w= -sq64 [k][c]) == ROOK || w == QUEEN) return true;
      if (w) break;
   }
   for (k = c+1; k < N; k++) {
      if ((w = -sq64 [l][k]) == ROOK || w == QUEEN) return true;
      if (w) break;
   }
   for (k = c-1; k >= 0; k--) {
      if ((w = -sq64 [l][k]) == ROOK || w == QUEEN) return true;
      if (w) break;
   }

   // fou ou reine menace
   for (k = 0; k < MIN (7-l, 7-c); k++) {       // vers haut droit
      if ((w = -sq64 [l+k+1][c+k+1]) == BISHOP || w == QUEEN) return true;
      if (w) break;
   }
   for (k = 0; k < MIN (7-l, c); k++) {         // vers haut gauche
      if ((w = -sq64 [l+k+1][c-k-1]) == BISHOP || w == QUEEN) return true;
      if (w) break;
   }
   for (k = 0; k < MIN (l, 7-c); k++) {         // vers bas droit
      if ((w = -sq64 [l-k-1][c+k+1]) == BISHOP || w == QUEEN) return true;
      if (w) break;
   }
   for (k = 0; k < MIN (l, c); k++) {           // vers bas gauche
      if ((w = -sq64 [l-k-1] [c-k-1]) == BISHOP || w == QUEEN) return true;
      if (w) break;
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
      if ((w = sq64 [k][c]) == ROOK || w == QUEEN) return true;
      if (w) break; // si w != 0...
   }
   for (k = l-1; k >= 0; k--) {
      if ((w = sq64 [k][c]) == ROOK || w == QUEEN) return true;
      if (w) break;
   }
   for (k = c+1; k < N; k++) {
      if ((w = sq64 [l][k]) == ROOK || w == QUEEN) return true;
      if (w) break;
   }
   for (k = c-1; k >= 0; k--) {
      if ((w = sq64 [l][k]) == ROOK || w == QUEEN) return true;
      if (w) break;
   }

   // fou ou reine menace
   for (k = 0; k < MIN (7-l, 7-c); k++) {       // vers haut droit
      if ((w = sq64 [l+k+1][c+k+1]) == BISHOP || w == QUEEN) return true;
      if (w) break;
   }
   for (k = 0; k < MIN (7-l, c); k++) {         // vers haut gauche
      if ((w = sq64 [l+k+1][c-k-1]) == BISHOP || w == QUEEN) return true;
      if (w) break;
   }
   for (k = 0; k < MIN (l, 7-c); k++) {         // vers bas droit
      if ((w = sq64 [l-k-1][c+k+1]) == BISHOP || w == QUEEN) return true;
      if (w) break;
   }
   for (k = 0; k < MIN (l, c); k++) {           // vers bas gauche
      w = sq64 [l-k-1] [c-k-1];
      if ((w = sq64 [l-k-1] [c-k-1]) == BISHOP || w == QUEEN) return true;
      if (w) break;
   }
   return false;
}

bool LCkingInCheck (TGAME sq64, int who, int l, int c) { /* */
   /* vrai si le roi situe case l, c est echec au roi */
   /* "who" est la couleur du roi qui est attaque */
   return (who == 1) ? LCBlackKingInCheck (sq64, l, c) : LCWhiteKingInCheck (sq64, l, c);
}

inline int8_t *pushList (TGAME refJeu, TLIST list, int nListe, int8_t *pl, int l1, int c1, int l2, int c2, int u) { /* */
   /* pousse le jeu refJeu dans la liste avec les modifications specifiees */
   // asm volatile ("movl $8, %%ecx; rep movsq" : "+D" (pl) : "S"(refJeu) : "ecx", "cc", "memory");
   memcpy (pl, refJeu, GAMESIZE);
   list [nListe][l2][c2] = u;
   list [nListe][l1][c1] = 0;
   return (pl + GAMESIZE); 
}

int buildListEnPassant (TGAME refJeu, int who, char *epGamer, TLIST list, int nextL) { /* */
   /* apporte le complement de positions a buildList prenant en compte en Passant suggere par le joueur */
   int8_t *pl = &list [nextL][0][0];
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

int buildList (TGAME refJeu, register int who, bool kingSide, bool queenSide, TLIST list) { /* */
   /* construit la liste des jeux possibles a partir de jeu. */
   /* 'who' joue */
   /* kingSide vrai si roque autorise cote roi */
   /* quenSide vrai si roque autorise cote roi */
   register int u, v, w, k, l, c;
   register int nListe = 0;
   int8_t *pl = &list [0][0][0];
   int8_t *pr = &refJeu [0][0];
   int base = (who == -1) ? 0 : 7;
   // info.nBuildListCall += 1;
  
   if (who * refJeu [base][4] == KING) {
      // roque cote reine
      if (queenSide && (who * refJeu [base][0]) == ROOK && 
         (refJeu [base][1] == 0) && (refJeu [base][2] == 0) && (refJeu [base][3] == 0) &&
         !LCkingInCheck (refJeu, who, base, 3) && !LCkingInCheck (refJeu, who, base, 4)) {
         // la case traversee par le roi et le roi ne sont pas echec au roi
         memcpy (pl, refJeu, GAMESIZE);
         list [nListe][base][0] = 0;
         list [nListe][base][2] = who * CASTLEKING;
         list [nListe][base][3] = who * ROOK;
         list [nListe++][base][4] = 0; 
         pl += GAMESIZE;
      }
      // Roque cote roi
      if (kingSide && (who * refJeu [base][7] == ROOK) && 
         (refJeu [base][5] == 0) && (refJeu [base][6] == 0) &&
         !LCkingInCheck (refJeu, who, base, 4) && !LCkingInCheck (refJeu, who, base, 5)) {
         // la case traversee par le roi et le roi ne sont pas echec au roi
         memcpy (pl, refJeu, GAMESIZE);
         list [nListe][base][4] = 0;
         list [nListe][base][5] = who * ROOK;
         list [nListe][base][6] = who * CASTLEKING;
         list [nListe++][base][7] = 0;
         pl += GAMESIZE;
      }
   }
    
   for (register int z = 0; z < GAMESIZE; z++) { 
      u = *(pr++);      // u est la valeur courante dans refJeu
      v = who * u;      // v = abs (u)
      if (v > 0) {
         l = LINE (z);
         c = COL (z);  
         switch (v) {   // v est la valeur absolue car u < 0 ==> who < 0
         case PAWN:
         // deplacements du pion
            if (who == -1 && l == 1 && 0 == refJeu [l+1][c] && 0 == refJeu[l+2][c]) {  // coup initial saut de 2 cases 
               pl = pushList (refJeu, list, nListe++, pl, l, c, l+2, c, -PAWN);
            }
            if (who == 1 && l == 6 && 0 == refJeu [l-1][c] && 0 == refJeu [l-2][c]) {  // coup initial saut de 2 cases
               pl = pushList (refJeu, list, nListe++, pl, l, c, l-2, c, PAWN);
            }
            if (((l-who) >= 0) && ((l-who) < 8) && (0 == refJeu [l-who][c])) {         // normal
               if (l-1 == 0 && who == 1) pl = pushList (refJeu, list, nListe++, pl, l, c, l-1, c, QUEEN);
               else if (l+1 == 7 && who == -1) pl = pushList (refJeu, list, nListe++, pl, l, c, l+1, c, -QUEEN);
               else pl = pushList (refJeu, list, nListe++, pl, l, c, l-who, c, PAWN * who);
            }
            // prise a droite
            if (c < 7 && (l-who) >=0 && (l-who) < N && refJeu [l-who][c+1]*who < 0) {  // signes opposes
               if (l-1 == 0 && who == 1) pl = pushList (refJeu, list, nListe++, pl, l, c, l-1, c+1, QUEEN);
               else if (l+1 == 7 && who == -1) pl = pushList (refJeu, list, nListe++, pl, l, c, l+1, c+1, -QUEEN);
               else pl = pushList (refJeu, list, nListe++, pl, l, c, l-who, c+1, PAWN * who);
            }
            // prise a gauche
            if (c > 0 && (l-who) >= 0 && (l-who) < N && refJeu [l-who][c-1]*who < 0) { // signes opposes
               if (l-1 == 0 && who == 1) pl = pushList (refJeu, list, nListe++, pl, l, c, l-1, c-1, QUEEN);
               else if (l+1 == 7 && who == -1) pl = pushList (refJeu, list, nListe++, pl, l, c, l+1, c-1, -QUEEN);
               else pl = pushList (refJeu, list, nListe++, pl, l, c, l-who, c-1, PAWN * who);
            }
            break;
         case KING: case CASTLEKING:
            if (c < 7 && (u * refJeu [l][c+1] <= 0))
               pl = pushList (refJeu, list, nListe++, pl, l, c, l, c+1, who * CASTLEKING);
            if (c > 0 && (u * refJeu [l][c-1] <= 0))
               pl = pushList (refJeu, list, nListe++, pl, l, c, l, c-1, who * CASTLEKING);
            if (l < 7 && (u * refJeu [l+1][c] <= 0))
               pl = pushList (refJeu, list, nListe++, pl, l, c, l+1, c, who * CASTLEKING);
            if (l > 0 && (u * refJeu [l-1][c] <= 0))
               pl = pushList (refJeu, list, nListe++, pl, l, c, l-1, c, who * CASTLEKING);
            if ((l < 7) && (c < 7) && (u * refJeu [l+1][c+1] <= 0))
               pl = pushList (refJeu, list, nListe++, pl, l, c, l+1, c+1, who * CASTLEKING);
            if ((l < 7) && (c > 0) && (u * refJeu [l+1][c-1] <= 0))
               pl = pushList (refJeu, list, nListe++, pl, l, c, l+1, c-1, who * CASTLEKING);
            if ((l > 0) && (c < 7) && (u * refJeu [l-1][c+1] <= 0))
               pl = pushList (refJeu, list, nListe++, pl, l, c, l-1, c+1, who * CASTLEKING);
            if ((l > 0) && (c > 0) && (u * refJeu [l-1][c-1] <= 0))
               pl = pushList (refJeu, list, nListe++, pl, l, c, l-1, c-1, who * CASTLEKING);
            break;

         case KNIGHT:
            if (l < 7 && c < 6 && (u * refJeu [l+1][c+2] <= 0)) 
               pl = pushList (refJeu, list, nListe++, pl, l, c, l+1, c+2, u);
            if (l < 7 && c > 1 && (u * refJeu [l+1][c-2] <= 0))
               pl = pushList (refJeu, list, nListe++, pl, l, c, l+1, c-2, u);
            if (l < 6 && c < 7 && (u * refJeu [l+2][c+1] <= 0))
               pl = pushList (refJeu, list, nListe++, pl, l, c, l+2, c+1, u);
            if (l < 6 && c > 0 && (u * refJeu [l+2][c-1] <= 0))
               pl = pushList (refJeu, list, nListe++, pl, l, c, l+2, c-1, u);
            if (l > 0 && c < 6 && (u * refJeu [l-1][c+2] <= 0))
               pl = pushList (refJeu, list, nListe++, pl, l, c, l-1, c+2, u);
            if (l > 0  && c > 1 && (u * refJeu [l-1][c-2] <= 0))
               pl = pushList (refJeu, list, nListe++, pl, l, c, l-1, c-2, u);
            if (l > 1 && c < 7 && (u * refJeu [l-2][c+1] <= 0))
               pl = pushList (refJeu, list, nListe++, pl, l, c, l-2, c+1, u);
            if (l > 1 && c > 0 && (u * refJeu [l-2][c-1] <= 0))
               pl = pushList (refJeu, list, nListe++, pl, l, c, l-2, c-1, u);
            break;

         case ROOK: case QUEEN:
            for (k = l+1; k < N; k++) {   // en haut
               if ((w = refJeu [k][c]) * u  <= 0) {
                  pl = pushList (refJeu, list, nListe++, pl, l, c, k, c, u);
                  if (w)  break;          // si w != 0...
               }
               else break;
            }
            for (k = l-1; k >=0; k--) {   // en bas
               if ((w = refJeu [k][c]) * u <= 0) {
                  pl = pushList (refJeu, list, nListe++, pl, l, c, k, c, u);
                  if (w) break;
               }
               else break;
            }
            for (k = c+1; k < N; k++) {   // a droite
               if ((w = refJeu [l][k]) * u <= 0) {
                  pl = pushList (refJeu, list, nListe++, pl, l, c, l, k, u);
                  if (w) break;
               }
               else break;
            }
            for (k = c-1; k >=0; k--)  {  // a gauche
               if ((w = refJeu [l][k]) * u <= 0) {
                  pl = pushList (refJeu, list, nListe++, pl, l, c, l, k, u);
                  if (w) break;
               }
               else break;
            }
            if (v == ROOK) break;
         // surtout pas de break pour la reine
         case BISHOP :                             // valable aussi pour QUEEN
            for (k = 0; k < MIN (7-l, 7-c); k++) { // vers haut droit
               if ((w = refJeu [l+k+1][c+k+1]) * u <= 0) {
                  pl = pushList (refJeu, list, nListe++, pl, l, c, l+k+1, c+k+1, u);
                  if (w) break;
               }
               else break;
            }
            for (k = 0; k < MIN (7-l, c); k++) {   // vers haut gauche
               if ((w = refJeu [l+k+1][c-k-1]) * u <= 0) {
                  pl = pushList (refJeu, list, nListe++, pl, l, c, l+k+1, c-k-1, u);
                  if (w) break;
               }
               else break;
            }
            for (k = 0; k < MIN (l, 7-c); k++) {   // vers bas droit
               if ((w = refJeu [l-k-1][c+k+1]) * u <= 0) {
                  pl = pushList (refJeu, list, nListe++, pl, l, c, l-k-1, c+k+1, u);
                  if (w) break;
               }
               else break;
            }
            for (k = 0; k < MIN (l, c); k++) {     // vers bas gauche
               if ((w = refJeu [l-k-1][c-k-1]) * u  <= 0) {
                  pl = pushList (refJeu, list, nListe++, pl, l, c, l-k-1, c-k-1, u);
                  if (w) break;
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
   register int maxList = buildList (sq64, who, true, true, list);
   if (maxList == 0) return true;
   for (register int k = 0; k < maxList; k++)
      if (! fKingInCheck (list [k], who)) return false;
   return true;
}

int evaluation (TGAME sq64, int who, bool *pat) { /* */
   /* fonction d'evaluation retournant MAT si Ordinateur gagne, */
   /* -MAT si joueur gagne, 0 si nul,... */
   /* position le boolean pat si pat */
   register int l, c, v, eval;
   int8_t *p64 = &sq64 [0][0];
   int lwho, cwho, ladverse, cadverse, nBBishops, nWBishops;
   bool kingInCheck;
   *pat = false;
   lwho = cwho = ladverse = cadverse = 0;
   eval = 0;
   nBBishops = nWBishops = 0;  // nombre ce fous Black, White.
   info.nEvalCall += 1;
   for (register int z = 0; z < GAMESIZE; z++) {
      // eval des pieces
      if ((v = (*p64++)) == 0) continue;
      // v est la valeur courante
      l = LINE (z);
      c = COL (z);
      eval += ((v > 0) ? val [v] : -val [-v]);
      // bonus si cavalier fou tour reine dans le carre central
      if ((c == 3 || c == 4) && (l == 3 || l == 4)) {
         if (v > PAWN && v < KING) eval += BONUSCENTER;
         else if (v < -PAWN && v > -KING) eval -= BONUSCENTER;
      }
       
      switch (v) {
      case KING : case CASTLEKING : // on repere ou est le roi
         if (who == 1) { lwho = l; cwho = c; }
         else  { ladverse = l; cadverse = c; }
         break;
      case -KING: case -CASTLEKING :
         if (who == 1) { ladverse = l; cadverse = c; }
         else  {lwho = l; cwho = c;}
         break;
      case KNIGHT:   // on privilégie cavaliers au centre
         if (c >= 2 && c <= 5 && l >= 2 && l <= 5) eval += BONUSKNIGHT;
         break;
      case -KNIGHT:   
         if (c >= 2 && c <= 5 && l >= 2 && l <= 5) eval -= BONUSKNIGHT;
         break;
      case BISHOP:   // on attribue un bonus si deux fous de la meme couleur
         nBBishops += 1;
         break;
      case -BISHOP:
         nWBishops += 1;
         break;
      case ROOK:     // bonus si tour mobile
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
      case PAWN: // pion noir
         // plus on est prets de la ligne 0, mieux c'est
         eval += ((N - 1) - l) * BONUSPAWNAHEAD;
         // si pion derriere un autre pion (noir ou blanc), pas bon.
         if (l > 0 && (abs (sq64 [l-1][c])) == PAWN) eval -= MALUSBLOCKEDPAWN;
         // si pion n'a pas d'ami à droite, gauche et devant-droite ou gauche, pas bon
         if (l > 0 && c > 1 && c < 7 && (sq64 [l-1][c-1] < PAWN) && 
            (sq64 [l-1][c+1] < PAWN) && (sq64 [l][c-1] < PAWN) && (sq64 [l][c+1] < PAWN))
            eval -= MALUSISOLATEDPAWN; 
         break;
      case -PAWN: // pion blanc : pareil sauf que c'est le contraire
         eval -= l * BONUSPAWNAHEAD;
         if (l < 7 && (abs (sq64 [l+1][c])) == PAWN) eval -= MALUSBLOCKEDPAWN;
         if (l < 7 && c > 1 && c < 7 && (sq64 [l+1][c-1] > -PAWN) && 
            (sq64 [l+1][c+1] != -PAWN) && (sq64 [l][c-1] > -PAWN) && (sq64 [l][c+1] > -PAWN))
            eval += MALUSISOLATEDPAWN; 
         break;
      default:;
      }
   }
   // printf ("eval 1 : %d\n", eval);
   // printf ("lwho: %d, cwho : %d, ladverse : %d, cadverse : %d\n", lwho, cwho, ladverse, cadverse);
   if (nBBishops >= 2) eval += BONUSBISHOP;
   if (nWBishops >= 2) eval -= BONUSBISHOP;
   if (LCkingInCheck (sq64, who, lwho, cwho)) return -who * MATE; // who ne peut pas jouer et se mettre en echec
   kingInCheck = LCkingInCheck (sq64, -who, ladverse, cadverse);
   if (kingCannotMove (sq64, -who)) { 
      if (kingInCheck) return who * MATE;
      else {
         *pat = true;
         return 0;   // Pat a distinguer de "0" avec le boolen pat
      }
   }
   if (kingInCheck) eval += who * KINGINCHECKEVAL;
   return eval;
}

int alphaBeta (TGAME sq64, int who, int p, int refAlpha, int refBeta) { /* */
   /* le coeur du programme */
   info.nAlphaBeta += 1;
   TLIST list;
   int maxList = 0;
   int k, note;
   bool pat = false;
   int val;
   int alpha = refAlpha;
   int beta = refBeta;

   if (getInfo.trans) {
      uint64_t zobrist = computeHash (sq64, who);
      uint32_t hash = zobrist & MASQMAXTRANSTABLE; 
      uint32_t check = zobrist >> 32; 
      if (trTa [hash].used && (trTa [hash].p <= p)) { 
         if ((trTa [hash].check == check) && (sameParity (trTa [hash].p, p))) {
            info.nbMatchTrans += 1;
            return (trTa [hash].eval);
         }
         else info.nbColl += 1;    // détection de collisions
      }
      trTa [hash].eval = note = evaluation (sq64, who, &pat);
      trTa [hash].p = p;
      trTa [hash].used = true;
      trTa [hash].check = check;
      info.nbTrTa += 1;
   }
   else { 
      note = evaluation (sq64, who, &pat);
   }
   if (info.calculatedMaxDepth < p) info.calculatedMaxDepth = p;

   // conditions de fin de jeu
   if (note == MATE) return MATE - p;   // -p pour favoriser le choix avec faible profondeur
   if (note == -MATE) return -MATE + p; // +p idem
   if (pat) return 0;
   if (p >= info.maxDepth) return note;
   // pire des notes a ameliorer
   if (who == 1) {
      val = MATE;
      maxList = buildList (sq64, -1, true, true, list);
      for (k = 0; k < maxList; k++) {
         note = alphaBeta (list [k], -1, p+1, alpha, beta);
         if (note < val) val = note;   // val = minimum...
         if (alpha > val) return val;
         if (val < beta) beta = val;   // beta = MIN (beta, val);
      }
   }
   else {
   // recherche du maximum
      val = -MATE;
      maxList = buildList (sq64, 1, true, true, list);
      for (k = 0; k < maxList; k++) {
         note = alphaBeta (list [k], 1, p+1, alpha, beta);
         if (note > val) val = note;   // val = maximum...
         if (beta < val) return val;
         if (val > alpha) alpha = val; // alpha = max (alpha, val);
      }
   }
   return val;
}

void *fThread (void *arg) { /* */
   /* association des thread a alphabeta */
   long k = (long) arg;
   info.moveList[k].eval = alphaBeta (list [k], -gamer.color, 0, -MATE, MATE);
   pthread_exit (NULL);
}

int comp (const void *a, const void *b) {
   /*comparaison evaluation de deux move pour tri croissant si couleur gamer blanche, decroissante sinon */
   const MOVELIST *pa = a;
   const MOVELIST *pb = b;
   return (gamer.color == WHITE) ? pb->eval - pa->eval : pa -> eval - pb -> eval;
}

int whereKings (TGAME sq64, int gamerColor, int *lGK, int *cGK, int *lCK, int *cCK) { /* */
   /* localise les rois */
   /* renvoie le nombre de pieces totales */
   *lGK = *cGK = *lCK = *cCK = -1;
   int v;
   int n = 0;
   for (int l = 0; l < N; l++) {
      for (int c = 0; c < N; c++) {
         v = - sq64 [l][c] * gamerColor;
         if (v > 0) n += 1;
         else if (v < 0) n += 1;
         if (v == KING || v == CASTLEKING) {
            *lCK = l;
            *cCK = c;
         }
         if (v == -KING || v == -CASTLEKING) {
            *lGK = l;
            *cGK = c;
         }
      }
   }
   return n;
}

int computerPlay () { /* */
   /* retourne 0 ou indication status */
   /* sq64 est le jeu en variable globale */
   /* construit la liste des jeux possibles et selon le cas */
   /* appel de la biblio ouverture openingAll */
   /* appel des tables de fin de jeux syzygy */
   /* appel alphaBeta en multithread */
   enum {INIT, OUV, ENDGAME, ALPHABETA} status = INIT;
   char fen [MAXBUFFER] = "";
   pthread_t tThread [MAXTHREADS];
   int bestNote = gamer.color * MATE; // la pire des notes possibleq
   int i;
   long k;
   char trash [100];
   bool bTrash;
   TGAME localSq64;
   struct timeval tRef;
   int lGK, cGK, lCK, cCK; // ligne colonnes Gamer et Computer King 
   lGK = cGK = lCK = cCK = -1,
   info.wdl = -1;                    // valeur inatteignable montrant que syzygy n'a pas ete appelee
   info.nbThread = (getInfo.multi) ? sysconf (_SC_NPROCESSORS_CONF) : 1;
   computer.kingState = gamer.kingState = NOEXIST;
   info.score = ERROR;
   info.nPieces = whereKings (sq64, gamer.color, &lGK, &cGK, &lCK, &cCK);
   if ((lGK == -1) || (lCK == -1)) {
      strcpy (info.comment, "ERR: No king");
      return 0; // manque de roi
   } 
   computer.kingState = gamer.kingState = EXIST;
   memcpy (localSq64, sq64, GAMESIZE);
   if (getInfo.trans) {
      if ((trTa = calloc (MAXTRANSTABLE, sizeof (StrTa))) == NULL) {
         strcpy (info.comment, "ERR: malloc in computerPlay ()\n");
         return (0);
      }
      initTable();                     // pour table hachage Zobrist
   }

   // check etat du joueur
   if (LCkingInCheck (sq64, gamer.color, lGK, cGK)) {
      if (kingCannotMove(sq64, gamer.color)) gamer.kingState = ISMATE;
      else gamer.kingState = UNVALIDINCHECK;
      info.score = ERROR;
      return 0; // le joueur n'a pas le droit de se presenter en echec apres avoir joue
   }
   // check du computer
   if (LCkingInCheck(sq64, -gamer.color, lCK, cCK))
      computer.kingState = ISINCHECK;
   if (kingCannotMove(sq64, -gamer.color)) {
      if (computer.kingState == ISINCHECK) {
         computer.kingState = ISMATE;
         info.score = (gamer.color == WHITE) ? WHITEWIN : BLACKWIN;
      }
      else {
         computer.kingState = ISPAT;
         info.score = DRAW;
         return 0;
      }
   }
   gettimeofday (&tRef, NULL);
   info.computeTime = tRef.tv_sec * MILLION + tRef.tv_usec;
   info.nClock = clock ();
   strcpy (info.comment, "");
   gamer.nValidPos = buildList (sq64, gamer.color, gamer.kingCastleOK, gamer.queenCastleOK, list);
   nextL = buildList (sq64, -gamer.color, computer.kingCastleOK, computer.queenCastleOK, list);
   nextL = buildListEnPassant (sq64, -gamer.color, gamer.ep, list, nextL);
   computer.nValidPos = nextL;
   if (nextL == 0) {
      strcpy (info.comment, "ERR: no move. Strange");
      return 0; // impossible de jouer... Bizarre
   }
   info.maxDepth = fMaxDepth (getInfo.level, gamer.nValidPos, computer.nValidPos);
   // memorisation de tous les deplacements possibles pour envoi
   for (k = 0; k < nextL; k++) {
      difference (sq64, list [k], -gamer.color, &info.lastCapturedByComputer, info.moveList [k].move, 
         trash, trash, trash, &bTrash, &bTrash);
      memcpy (info.moveList [k].jeu, list [k], GAMESIZE);
   }

   // lancement de la recherche par l'ordi
   gameToFen (sq64, fen, -gamer.color, ' ', false, computer.ep, 0, 0);
   // recherche de fin de partie voir https://syzygy-tables.info/
   if ((info.nPieces) <= MAXPIECESSYZYGY) {
      sprintf (fen, "%s - - %d %d", fen, info.cpt50, info.nb); // pour regle des 50 coups
      if (syzygyRR (PATHTABLE, fen, &info.wdl, info.move, info.comment)) {
         moveGame (sq64, -gamer.color, info.move);
	      status = OUV;
      }
   }
   else {
      // ouvertures
      if ((info.nb < MAXNBOPENINGS) &&
         (openingAll (OPENINGDIR, (gamer.color == WHITE) ? ".b.fen": ".w.fen", fen, info.comment, info.move))) {
            moveGame (sq64, -gamer.color, info.move);
            status = ENDGAME;
         }
      }
   if (status == INIT) {
      // recherche par la methode minimax Alphabeta
      if (getInfo.multi) { // Multi thread
         for (k = 0; k < nextL; k++)
            if (pthread_create (&tThread [k], NULL, fThread, (void *) k)) {
               strcpy (info.comment, "ERR: pthread_create");
               return 0;
            }
         for (k = 0; k < nextL; k++)
            if (pthread_join (tThread [k], NULL)) {
               strcpy (info.comment, "ERR: pthread_join");
               return 0;
            }
      }
      else {
         for (k = 0; k < nextL; k++) info.moveList[k].eval = alphaBeta (list [k], -gamer.color, 0, -MATE, MATE);
      }
      // info.movList contient les évaluations de toutes les possibilites
      // recherche de la meilleure note
      status = ALPHABETA;
/*for (int i = 0; i < 10; i++) {
   printGame (info.moveList[i].jeu, info.moveList[i].eval);
   printf ("move : %s\n", info.moveList[i].move);
} */
      qsort (info.moveList, nextL, sizeof (MOVELIST), comp); // tri croisant ou decroissant selon couleur gamer
      info.evaluation = bestNote = info.moveList [0].eval;
      i = 0;
      while ((info.moveList[i].eval == bestNote) && (i < nextL)) i += 1; // il y a i meilleurs jeux
      k = (getInfo.alea) ? rand () % i : 0; // indice du jeu choisi au hasard OU premier
      info.nBestNote = i;
      memcpy (sq64, info.moveList[k].jeu, GAMESIZE);
   }
   if (getInfo.trans) free (trTa);
   difference (localSq64, sq64, -gamer.color, &info.lastCapturedByComputer, info.computerPlayC, 
      info.computerPlayA, gamer.ep, computer.ep, &computer.queenCastleOK, &computer.kingCastleOK);
   if (gamer.color == WHITE) info.nb += 1;
   if (abs (info.computerPlayC [0] == 'P') ||  info.computerPlayC [3] == 'x') // si un pion bouge ou si prise
      info.cpt50 = 0;
   else info.cpt50 += 1;
   
   info.nClock = clock () - info.nClock;
   gettimeofday (&tRef, NULL);
   info.computeTime = tRef.tv_sec * MILLION + tRef.tv_usec - info.computeTime;
  
   // analyse resultat 
   computer.kingState = gamer.kingState = NOEXIST;
   info.score = ERROR;
   info.nPieces = whereKings (sq64, gamer.color, &lGK, &cGK, &lCK, &cCK);
   if ((lGK == -1) || (lCK == -1)) {
      strcpy (info.comment, "ERR: No king");
      return 0; // manque de roi 
   }
   computer.kingState = gamer.kingState = EXIST;
   info.score = ONGOING;

   if (LCkingInCheck (sq64, gamer.color, lGK, cGK)) gamer.kingState = ISINCHECK;
   if (kingCannotMove (sq64, gamer.color)) {
      if (gamer.kingState == ISINCHECK) {
         gamer.kingState = ISMATE;
         info.score = (gamer.color == WHITE) ? BLACKWIN : WHITEWIN;
      }
      else {
         gamer.kingState = ISPAT;
         info.score = DRAW;
      }
   }

   if (LCkingInCheck (sq64, -gamer.color, lCK, cCK)) { // cas bizarre
      computer.kingState = UNVALIDINCHECK;
      info.score = ERROR;
   }
   if (info.cpt50 > 50) info.score = DRAW;
   return status;
}

bool cgi () { /* */
   /* MODE CGI */
   /* gere le fichier log */
   /* lit les variables d'environnement, lance computerPlay */
   /* vrai si on peut lire les variables d'environnement */
   /* faux sinon */
   char fen [MAXLENGTH];
   char temp [MAXBUFFER];
   char *str;
   char *env;
   str = temp;
   char buffer [80] = "";
   
   // log date heure et adresse IP du joueur
   time_t now = time (NULL); // pour .log
   struct tm *timeNow = localtime (&now);
   flog = fopen (F_LOG, "a");       // preparation du fichier log 
   if ((env = getenv ("REMOTE_ADDR")) == NULL) return false;
   strftime (buffer, 80, "%F; %T", timeNow);
   fprintf (flog, "%s; ", buffer);           // log de la date et du temps
   env = buffer;
   fprintf (flog, "%s; ", env);              // log de l'address IP distante
   if ((env = getenv ("HTTP_USER_AGENT")) == NULL) return false;
   if (strlen (env) > 8) *(env + 8) = '\0';  // tronque a x caracteres
   fprintf (flog, "%s; ", env);              // log du user agent distant

   // Lecture de la chaine de caractere representant le jeu via la methode post

   if ((env = getenv ("QUERY_STRING")) == NULL) return false;  // Les variables

   if ((str = strstr (env, "nomulti")) != NULL) getInfo.multi = false;
   if ((str = strstr (env, "noalea")) != NULL) getInfo.alea = false;
   if ((str = strstr (env, "noalea")) != NULL) getInfo.alea = false;
   if ((str = strstr (env, "notrans")) != NULL) getInfo.trans = false;
   if ((str = strstr (env, "fen=")) != NULL)
      sscanf (str, "fen=%[a-zA-Z0-9+-/]", getInfo.fenString);
   if ((str = strstr (env, "level=")) != NULL)
      sscanf (str, "level=%d", &getInfo.level);
   if ((str = strstr (env, "reqType=")) != NULL)
      sscanf (str, "reqType=%d", &getInfo.reqType);
 
   if (getInfo.reqType != 0) {                 // on lance le jeu
       gamer.color = -fenToGame (getInfo.fenString, sq64, gamer.ep, &info.cpt50, &info.nb);
       computerPlay ();
       gameToFen (sq64, fen, gamer.color, '+', true, computer.ep, info.cpt50, info.nb);
       sendGame (true, fen, getInfo.reqType);
       fprintf (flog, "%2d; %s; %s; %d", getInfo.level, getInfo.fenString, info.computerPlayC, info.evaluation);
   }
   else sendGame (true, "", getInfo.reqType);
   fprintf (flog, "\n");
   fclose (flog);
   return true;
}

int main (int argc, char *argv[]) { /* */
   /* lit la ligne de commande */
   /* si une option "-x" existe on l'execute */
   /* si pas d'argument alors CGI */
   char fen [MAXLENGTH];
   srand (time (NULL));             // initialise le generateur aleatoire

   // si pas de parametres on va au cgi (fin de main)
   if (argc >= 2 && argv [1][0] == '-') { // si il y a des parametres. On choidi un test
      if (argc > 2) strcpy (getInfo.fenString, argv [2]);
      if (argc > 3) getInfo.level = atoi (argv [3]);
      gamer.color = -fenToGame (getInfo.fenString, sq64, gamer.ep, &info.cpt50, &info.nb);
      switch (argv [1][1]) {
      case 'q': case 'v': // q quiet, v verbose
         printf ("argv1: %s\n", argv [1]);
         if (strlen (argv [1]) > 2) getInfo.trans = (argv [1][2] != 'n');
         if (strlen (argv [1]) > 3) getInfo.alea = (argv [1][3] != 'n');
         if (strlen (argv [1]) > 4) getInfo.multi = (argv [1][4] != 'n');
         computerPlay ();
         if (argv [1][1] == 'v') 
            printGame (sq64, evaluation (sq64, -gamer.color, &info.pat));
         gameToFen (sq64, fen, gamer.color, '+', true, computer.ep, info.cpt50, info.nb); 
         sendGame (false, fen, getInfo.reqType);
         break;
      case 'f': //performance
         info.nClock = clock ();
         for (int i = 0; i < getInfo.level * MILLION; i++)
            nextL = buildList (sq64, -gamer.color, true, true, list);         
         printf ("buildList. clock: %lf\n", (double) (clock () - info.nClock)/CLOCKS_PER_SEC);
         break;
      case 't': // tests
         printf ("sizeof trta : %ld\n", sizeof (StrTa));
         break;
      case 'd': case 'D': // display
         if (argv [1][1] == 'D') printGame (sq64, evaluation (sq64, gamer.color, &info.pat));
         gameToFen (sq64, fen, gamer.color, '+', true, computer.ep, info.cpt50, info.nb); 
         printf ("{ \"fen\" : \"%s\"}\n",fen);
         break; 
      case 'm': case 'M': // move
         moveGame (sq64, gamer.color, argv [argc-1]);        
         if (argv [1][1] == 'M') printGame (sq64, evaluation (sq64, gamer.color, &info.pat));
         gameToFen (sq64, fen, gamer.color, '+', true, computer.ep, info.cpt50, info.nb); 
         printf ("{ \"fen\" : \"%s\"}\n",fen);
         break;
      default:
         printf ("%s\n", HELP);
      }
   }
   else 
   if ((argc <= 1) && cgi ()); // la voie normale : pas de parametre => cgi.
   else printf ("%s\n", HELP);
   exit (EXIT_SUCCESS);
}
