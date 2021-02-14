/*   Pour produire la doc sur les fonctions : grep "\/\*" chess.c | sed 's/^\([a-zA-Z]\)/\n\1/' */
/*   Jeu d'echec */
/*   ./chess.cgi -q |-v [FENstring] [profondeur] [exp] : CLI avec sortie JSON q)uiet v)erbose */
/*   ./chess.cgi -m | M [Fenstring] [Movestring] : execute le déplacement etrenvoie le jeur */
/*   ./chess.cgi -d | D [FENstring] : affiche le jeu */
/*   ./chess.cgi -f : test performance */
/*   ./chess.cgi -t [FENGame] : test unitaire */
/*   ./chess.cgi -h : help */
/*   sans parametre ni option :  CGI gérant une API restful (GET) avec les réponses au format JSON */
/*   fichiers associes : chess.log, chessB.fen, chessW.fen, chessUtil.c, syzygy.c, tbprobes.c tbcore.c et .h associes  */
/*   Structures de donnees essentielles */
/*     - jeu represente dans un table a 2 dimensions : TGAME sq64 */
/*     - liste des deplacement qui est un tableau de move : */
/*     - nextL est un entier pointant sur le prochain move a inserer dans la liste */
/*   Noirs : positifs  (Minuscules) */
/*   Blancs : negatifs  (Majuscules) */

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

#define LCkingInCheck(sq64,who,l,c)    ((who == 1) ? LCBlackKingInCheck (sq64, l, c) : LCWhiteKingInCheck (sq64, l, c))
#define MEMCPY(a,b)   asm volatile ("movl $8, %%ecx; rep movsq" : "+D" (a) : "S"(b) : "ecx", "cc", "memory")

// valorisation des pieces dans l'ordre PAWN KNIGHT BISHOP ROOK QUEEN KING CASTLEKING
// Le roi qui a deja roque a le code 7, le roi normal a le code 6
// tour = fou + 2 pions, dame >= fou + tour + pion
// le roi n'a pas de valeur. Le roi qui a roque a un bonus
// Voir fonction d'evaluation

const int valPiece [] = {0, 100, 300, 300, 500, 900, 0, BONUSCASTLE};  // pour fn evaluation

const struct Sprod { int v; int inc;                              // pour fMaxDepth
} valDepth [] = {{12, 7}, {25, 6}, {50, 5}, {100, 4}, {200, 3}, {400, 2},  {600, 1}};

const int8_t BLACKQUEENCASTLESERIE [5] = {0, 0, CASTLEKING, ROOK, 0};
const int8_t WHITEQUEENCASTLESERIE [5] = {0, 0, -CASTLEKING, -ROOK, 0};
const int8_t BLACKKINGCASTLESERIE  [4] = {0, ROOK, CASTLEKING, 0};
const int8_t WHITEKINGCASTLESERIE  [4] = {0, -ROOK, -CASTLEKING, 0};

FILE *flog;

struct {                           // description de la requete emise par le client
   char fenString [MAXLENGTH];     // le jeu
   int reqType;                    // le type de requete : 0 1 ou 2
   int level;                      // la profondeur de la recherche souhaitee
   int alea;      // 1 si on prend un jeu de facon aleatoire quand plusieurs solutions 0 si premier -1 si dernier
   bool trans;                     // vrai si on utilise les tables de transpo
   bool multi;                     // vrai si multithread
   int  exp;                       // nombre de bits a 1 pour le masque masqMaxTransTable = pow (2, exp) - 1;
} getInfo = {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR+w+KQkq", 2, 4, 1, true, true, 24}; // par defaut

uint64_t masqMaxTransTable;        // idem (2 puissance exp) - 1. Quasi constante initialisee dans computerPlay.

TGAME sq64;
TLISTMOVE listMove;
int nextL;                         // nombre total utilisé dans la file

typedef struct  {                  // tables de transposition
   int16_t eval;                   // derniere eval
   int8_t p;                       // profondeur
   int8_t used;                    // booleen sur 8 bits. Utilise
   int32_t check;                  // pour verif collisions
} StrTa;
StrTa *trTa = NULL;                // Ce pointeur va servir de tableau après l'appel du malloc

uint64_t ZobristTable[8][8][14];   // 14 combinaisons avec RoiRoque

uint64_t rand64 () { /* */
   /* retourne un nb aleatoire sur 64 bits */
   return ((((uint64_t) rand ()) << 34) ^ (((uint64_t) rand ()) << 26) ^ (((uint64_t) rand ()) << 18) ^ (rand ()));
}

inline int findBlackKing (TGAME sq64) {
   void *p;
   if ((p = memchr (sq64, KING, GAMESIZE)) == NULL) p = memchr (sq64, CASTLEKING, GAMESIZE);
   return ((int8_t *) p - &sq64[0][0]);
}

inline int findWhiteKing (TGAME sq64) {
   void *p;
   if ((p = memchr (sq64, -KING, GAMESIZE)) == NULL) p = memchr (sq64, -CASTLEKING, GAMESIZE);
   return ((int8_t *) p - &sq64[0][0]);
}

void initTable() { /* */
   /* Initializes the table Zobrist*/
   for (int l = 0; l < N; l++)
      for (int c = 0; c < N; c++)
         for (int k = 0; k < 14; k++) // 14 avec Roque
            ZobristTable[l][c][k] = rand64 ();
}
  
inline int indexOf (register int v) { /* */
   /* index d'une piece pour transposition table */
   return (v > 0) ? v - 1 : (-v + 6); // 0=pion noir, 13=roiroque blanc
}

uint64_t computeHash (TGAME sq64) { /* */
   /* Computes the hashvalue of a given game. Zobrist */
   int v;     // valeurs de -7 a 7. Case vide = 0. Pieces neg : blanches. Pos : noires.
   int piece; // valeurs de piece de 0 a 13. Case vide non representee
   uint64_t h = 0;
   for (int c = 0; c < N; c++)  {
      for (int l = 0; l < 8; l++) {
         if ((v = sq64[l][c]) != 0) { // Case vide non prise en compte
            piece = indexOf (v); // 0=pion noir, 13=roiroque blanc
            h ^= ZobristTable[l][c][piece];
          }
       }
    }
    // h est la valeur de Zobrist sur 64 bits. Que l'on va réduire à 32 bits.
    return (h); 
}
  
int fMaxDepth (int lev, int nGamerPos, int nComputerPos) { /* */
   /* renvoie la profondeur du jeu en fonction du niveau choisi et */
   /* de l'etat du jeu */
   int prod = nComputerPos * nGamerPos;
   for (int i = 0; i < (sizeof (valDepth)/(2 * sizeof (int))); i++)
      if (prod < valDepth [i].v) return valDepth [i].inc + lev;
   return lev;
}

bool LCBlackKingInCheck (TGAME sq64, register int l, register int c) { /* */
   /* vrai si le roi Noir situe case l, c est echec au roi */
   register int w, k;
   //info.nLCKingInCheckCall += 1;
   // roi adverse  menace.  Matche -KING et -CASTLEKING
   if ((l < 7 && (sq64 [l+1][c] <= -KING)) ||
       (l > 0 && (sq64 [l-1][c] <= -KING)) ||
       (c < 7 && (sq64 [l][c+1] <= -KING)) ||
       (c > 0 && (sq64 [l][c-1] <= -KING)) ||
       (l < 7 && c < 7 && (sq64 [l+1][c+1] <= -KING)) ||
       (l < 7 && c > 0 && (sq64 [l+1][c-1] <= -KING)) ||
       (l > 0 && c < 7 && ((w = (sq64 [l-1][c+1])) <= -KING || w == -PAWN)) ||
       (l > 0 && c > 0 && ((w = (sq64 [l-1][c-1])) <= -KING || w == -PAWN)) ||

   // cavalier menace
       (l < 7 && c < 6 && (sq64 [l+1][c+2] == -KNIGHT)) ||
       (l < 7 && c > 1 && (sq64 [l+1][c-2] == -KNIGHT)) ||
       (l < 6 && c < 7 && (sq64 [l+2][c+1] == -KNIGHT)) ||
       (l < 6 && c > 0 && (sq64 [l+2][c-1] == -KNIGHT)) ||
       (l > 0 && c < 6 && (sq64 [l-1][c+2] == -KNIGHT)) ||
       (l > 0 && c > 1 && (sq64 [l-1][c-2] == -KNIGHT)) ||
       (l > 1 && c < 7 && (sq64 [l-2][c+1] == -KNIGHT)) ||
       (l > 1 && c > 0 && (sq64 [l-2][c-1] == -KNIGHT))
      ) return true;
   
   // tour ou reine menace
   for (k = l+1; k < N; k++) {
      if ((w = sq64 [k][c]) == -ROOK || w == -QUEEN) return true;
      if (w) break;
   }
   for (k = l-1; k >= 0; k--) {
      if ((w = sq64 [k][c]) == -ROOK || w == -QUEEN) return true;
      if (w) break;
   }
   for (k = c+1; k < N; k++) {
      if ((w = sq64 [l][k]) == -ROOK || w == -QUEEN) return true;
      if (w) break;
   }
   for (k = c-1; k >= 0; k--) {
      if ((w = sq64 [l][k]) == -ROOK || w == -QUEEN) return true;
      if (w) break;
   }

   // fou ou reine menace
   for (k = 0; k < MIN (7-l, 7-c); k++) {       // vers haut droit
      if ((w = sq64 [l+k+1][c+k+1]) == -BISHOP || w == -QUEEN) return true;
      if (w) break;
   }
   for (k = 0; k < MIN (7-l, c); k++) {         // vers haut gauche
      if ((w = sq64 [l+k+1][c-k-1]) == -BISHOP || w == -QUEEN) return true;
      if (w) break;
   }
   for (k = 0; k < MIN (l, 7-c); k++) {         // vers bas droit
      if ((w = sq64 [l-k-1][c+k+1]) == -BISHOP || w == -QUEEN) return true;
      if (w) break;
   }
   for (k = 0; k < MIN (l, c); k++) {           // vers bas gauche
      if ((w = sq64 [l-k-1] [c-k-1]) == -BISHOP || w == -QUEEN) return true;
      if (w) break;
   }
   return false;
}

bool LCWhiteKingInCheck (TGAME sq64, register int l, register int c) { /* */
   /* vrai si le roi Blanc situe case l, c est echec au roi */
   register int w, k;
   //info.nLCKingInCheckCall += 1;
   // roi adverse  menace. >= KING marche KING et CASTLELING
   if ((l < 7 && (sq64 [l+1][c] >= KING)) ||
       (l > 0 && (sq64 [l-1][c] >= KING)) ||
       (c < 7 && (sq64 [l][c+1] >= KING)) ||
       (c > 0 && (sq64 [l][c-1] >= KING)) ||
       (l < 7 && c < 7 && ((w = sq64 [l+1][c+1]) >= KING || w == PAWN)) ||
       (l < 7 && c > 0 && ((w = sq64 [l+1][c-1]) >= KING || w == PAWN)) ||
       (l > 0 && c < 7 && (sq64 [l-1][c+1] >= KING)) ||
       (l > 0 && c > 0 && (sq64 [l-1][c-1] >= KING)) ||

   // cavalier menace
       (l < 7 && c < 6 && (sq64 [l+1][c+2] == KNIGHT)) ||
       (l < 7 && c > 1 && (sq64 [l+1][c-2] == KNIGHT)) ||
       (l < 6 && c < 7 && (sq64 [l+2][c+1] == KNIGHT)) ||
       (l < 6 && c > 0 && (sq64 [l+2][c-1] == KNIGHT)) ||
       (l > 0 && c < 6 && (sq64 [l-1][c+2] == KNIGHT)) ||
       (l > 0 && c > 1 && (sq64 [l-1][c-2] == KNIGHT)) ||
       (l > 1 && c < 7 && (sq64 [l-2][c+1] == KNIGHT)) ||
       (l > 1 && c > 0 && (sq64 [l-2][c-1] == KNIGHT))) return true;

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

inline int pushMove (TLISTMOVE listMove, register int piece, register int type, register int nList, register int l1, register int c1, register int l2, register int c2) { /* */
   /* pousse un deplacement dans la liste */
   listMove [nList].type = type;
   listMove [nList].piece = piece;
   listMove [nList].l1 = l1;
   listMove [nList].c1 = c1;
   listMove [nList].l2 = l2;
   listMove [nList].c2 = c2;
   return nList + 1;
}

inline uint64_t doMove (TGAME sq64, TMOVE move, register uint64_t zobrist) { /* */
   /* execute le deplacement */
   /* renvoie le nouveau zobrist */
   int base;
   move.taken = sq64 [move.l2][move.c2];
   switch (move.type) {
   case STD:
      if (move.taken == 0) {
         sq64 [move.l1] [move.c1] = 0;
         zobrist ^= ZobristTable[move.l1][move.c1][indexOf(move.piece)]; 
         sq64 [move.l2] [move.c2] = move.piece;
         zobrist ^= ZobristTable[move.l2][move.c2][indexOf(move.piece)]; 
      }
      else {
         sq64 [move.l1] [move.c1] = 0;
         zobrist ^= ZobristTable[move.l1][move.c1][indexOf(move.piece)]; 
         sq64 [move.l2] [move.c2] = 0;
         zobrist ^= ZobristTable[move.l2][move.c2][indexOf(move.taken)]; 
         sq64 [move.l2] [move.c2] = move.piece;
         zobrist ^= ZobristTable[move.l2][move.c2][indexOf(move.piece)]; 
      }
      break;
   case PROMOTION:
      if (move.taken == 0) {
         sq64 [move.l1] [move.c1] = 0;
         zobrist ^= ZobristTable[move.l1][move.c1][indexOf(SIG(move.piece)*PAWN)]; 
         sq64 [move.l2] [move.c2] = move.piece;
         zobrist ^= ZobristTable[move.l2][move.c2][indexOf(move.piece)]; 
      }
      else {
         sq64 [move.l1] [move.c1] = 0;
         zobrist ^= ZobristTable[move.l1][move.c1][indexOf(SIG(move.piece)*PAWN)]; 
         sq64 [move.l2] [move.c2] = 0;
         zobrist ^= ZobristTable[move.l2][move.c2][indexOf(move.taken)]; 
         sq64 [move.l2] [move.c2] = move.piece;
         zobrist ^= ZobristTable[move.l2][move.c2][indexOf(move.piece)]; 
      }
      break;
   case CHANGEKING:
      if (move.taken == 0) {
         sq64 [move.l1] [move.c1] = 0;
         zobrist ^= ZobristTable[move.l1][move.c1][indexOf(move.piece)];
         sq64 [move.l2] [move.c2] = SIG(move.piece) * CASTLEKING;
         zobrist ^= ZobristTable[move.l2][move.c2][indexOf(SIG(move.piece)*CASTLEKING)]; 
      }
      else {
         sq64 [move.l1] [move.c1] = 0;
         zobrist ^= ZobristTable[move.l1][move.c1][indexOf(move.piece)]; 
         sq64 [move.l2] [move.c2] = 0;
         zobrist ^= ZobristTable[move.l2][move.c2][indexOf(move.taken)]; 
         sq64 [move.l2] [move.c2] = SIG(move.piece) * CASTLEKING;
         zobrist ^= ZobristTable[move.l2][move.c2][indexOf(SIG(move.piece)*CASTLEKING)]; 
      }
      break;
   case ENPASSANT:
      sq64 [move.l1] [move.c1] = 0;
      zobrist ^= ZobristTable[move.l1][move.c1][indexOf(move.piece)]; 
      sq64 [move.l2] [move.c2] = move.piece;
      zobrist ^= ZobristTable[move.l2][move.c2][indexOf(move.piece)]; 
      sq64 [move.l1] [move.c2] = 0;
      zobrist ^= ZobristTable[move.l1][move.c2][indexOf(move.taken)]; 
      break;
   case QUEENCASTLESIDE:
      base = (move.piece <= WHITE) ? 0 : 7;
      sq64 [base][0] = 0;
      zobrist ^= ZobristTable[base][0][indexOf(SIG(move.piece)*ROOK)]; 
      sq64 [base][3] = SIG(move.piece) * ROOK;
      zobrist ^= ZobristTable[base][3][indexOf(SIG(move.piece)*ROOK)]; 
      sq64 [base][4] = 0;
      zobrist ^= ZobristTable[base][4][indexOf(SIG(move.piece)*KING)]; 
      sq64 [base][2] = SIG(move.piece) * CASTLEKING;
      zobrist ^= ZobristTable[base][2][indexOf(SIG(move.piece)*CASTLEKING)];
      break; 
   case KINGCASTLESIDE:
      base = (move.piece <= WHITE) ? 0 : 7;
      sq64 [base][7] = 0;
      zobrist ^= ZobristTable[base][7][indexOf(SIG(move.piece)*ROOK)]; 
      sq64 [base][5] = SIG(move.piece) * ROOK;
      zobrist ^= ZobristTable[base][5][indexOf(SIG(move.piece)*ROOK)]; 
      sq64 [base][4] = 0;
      zobrist ^= ZobristTable[base][4][indexOf(SIG(move.piece)*KING)]; 
      sq64 [base][6] = SIG(move.piece) * CASTLEKING;
      zobrist ^= ZobristTable[base][6][indexOf(SIG(move.piece)*CASTLEKING)];
      break; 
   default:;
   }
   // if (zobrist != computeHash (sq64)) {printf ("ERR In DoMove 2\n"); exit (0);};
   return zobrist;
}

inline int doMove0 (TGAME sq64, TMOVE move) { /* */
   /* execute le deplacement */
   /* renvoie la position du roi qui a bouge sous forme l * 8 + c sinon -1*/
   switch (move.type) {
   case STD:
      sq64 [move.l1] [move.c1] = 0;
      sq64 [move.l2] [move.c2] = move.piece;
      return (abs (move.piece) >= KING) ? ((move.l2 << 3) + move.c2) : -1;
   case PROMOTION:
      sq64 [move.l1] [move.c1] = 0;
      sq64 [move.l2] [move.c2] = move.piece;
      return -1;
   case CHANGEKING:
      sq64 [move.l1] [move.c1] = 0;
      sq64 [move.l2] [move.c2] = (move.piece >= BLACK) ? CASTLEKING : -CASTLEKING;
      return (move.l2 << 3) + move.c2;
   case ENPASSANT:
      sq64 [move.l2] [move.c2] = move.piece;
      sq64 [move.l1] [move.c1] = sq64 [move.l1] [move.c2] = 0;
      return -1;
   case QUEENCASTLESIDE:
      if (move.piece <= WHITE) {
         memcpy (&sq64[0][0], WHITEQUEENCASTLESERIE, 5);
         return 2;
      }
      else {
         memcpy (&sq64[7][0], BLACKQUEENCASTLESERIE, 4);
         return (N << 3) + 2;
      } 
   case KINGCASTLESIDE:
      if (move.piece <= WHITE) {
         memcpy (&sq64[0][4], WHITEKINGCASTLESERIE, 4);
         return 6;
      }
      else {
         memcpy (&sq64[7][4], BLACKKINGCASTLESERIE, 4);
         return (N << 3) + 6;
      } 
   default:;
   }
   return -1;
}

int buildListEnPassant (TGAME refJeu, int who, char *epGamer, TLISTMOVE listMove, int nextL) { /* */
   /* apporte le complement de positions a buildList prenant en compte en Passant suggere par le joueur */
   int nList = nextL;
   if (epGamer [0] == '-') return nList;
   int lEp = epGamer [1] - '1';
   int cEp = epGamer [0] - 'a';
   if ((cEp > 0) && (refJeu [lEp+who][cEp-1] == who * PAWN) &&      // vers droite
      (refJeu[lEp][cEp] == 0) && (refJeu [lEp+who][cEp] == -who * PAWN)) {
      nList = pushMove (listMove, who * PAWN, ENPASSANT, nList, lEp+who, cEp-1, lEp, cEp);
   }
   if ((cEp < N) && (refJeu [lEp+who][cEp+1] == who * PAWN) &&      // vers gauche
      (refJeu [lEp][cEp] == 0) && (refJeu [lEp+who][cEp] == -who * PAWN)) { 
      nList = pushMove (listMove, who * PAWN, ENPASSANT, nList, lEp+who, cEp+1, lEp, cEp);
   }
   return nList;
}

int buildList (TGAME refJeu, register int who, bool kingSide, bool queenSide, TLISTMOVE listMove) { /* */
   /* construit la liste des jeux possibles a partir de jeu. */
   /* 'who' joue */
   /* kingSide vrai si roque autorise cote roi */
   /* quenSide vrai si roque autorise cote roi */
   register int u, v, w, k, l, c;
   register int nList = 0;
   register int8_t *pr = &refJeu [0][0];
   int base = (who == WHITE) ? 0 : 7;
   int change;
   // info.nBuildListCall += 1;
  
   u = refJeu [base][4];
   if (who * refJeu [base][4] == KING) {
      // roque cote reine
      if (queenSide && (who * refJeu [base][0]) == ROOK && 
         (refJeu [base][1] == 0) && (refJeu [base][2] == 0) && (refJeu [base][3] == 0) &&
         !LCkingInCheck (refJeu, who, base, 3) && !LCkingInCheck (refJeu, who, base, 4)) {
         // la case traversee par le roi et le roi ne sont pas echec au roi
         nList = pushMove (listMove, u, QUEENCASTLESIDE, nList, 0, 0, 0, 0);
      }
      // Roque cote roi
      if (kingSide && (who * refJeu [base][7] == ROOK) && 
         (refJeu [base][5] == 0) && (refJeu [base][6] == 0) &&
         !LCkingInCheck (refJeu, who, base, 4) && !LCkingInCheck (refJeu, who, base, 5)) {
         // la case traversee par le roi et le roi ne sont pas echec au roi
         nList = pushMove (listMove, u, KINGCASTLESIDE, nList, 0, 0, 0, 0);
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
               nList = pushMove (listMove, u, 0, nList, l, c, l+2, c);
            }
            if (who == 1 && l == 6 && 0 == refJeu [l-1][c] && 0 == refJeu [l-2][c]) {  // coup initial saut de 2 cases
               nList = pushMove (listMove, u, 0, nList, l, c, l-2, c);
            }
            if (((l-who) >= 0) && ((l-who) < 8) && (0 == refJeu [l-who][c])) {         // normal
               if (l-1 == 0 && who == 1) nList = pushMove (listMove, QUEEN, PROMOTION, nList, l, c, l-1, c);
               else if (l+1 == 7 && who == -1) nList = pushMove (listMove, -QUEEN, PROMOTION, nList, l, c, l+1, c);
               else nList = pushMove (listMove, u, 0, nList, l, c, l-who, c);
            }
            // prise a droite
            if (c < 7 && (l-who) >=0 && (l-who) < N && refJeu [l-who][c+1]*who < 0) {  // signes opposes
               if (l-1 == 0 && who == 1) nList = pushMove (listMove, QUEEN, PROMOTION, nList, l, c, l-1, c+1);
               else if (l+1 == 7 && who == -1) nList = pushMove (listMove, -QUEEN, PROMOTION, nList, l, c, l+1, c+1);
               else nList = pushMove (listMove, u, 0, nList, l, c, l-who, c+1);
            }
            // prise a gauche
            if (c > 0 && (l-who) >= 0 && (l-who) < N && refJeu [l-who][c-1]*who < 0) { // signes opposes
               if (l-1 == 0 && who == 1) nList = pushMove (listMove, QUEEN, PROMOTION, nList, l, c, l-1, c-1);
               else if (l+1 == 7 && who == -1) nList = pushMove (listMove, -QUEEN, PROMOTION, nList, l, c, l+1, c-1);
               else nList = pushMove (listMove, u, 0, nList, l, c, l-who, c-1);
            }
            break;
         case KING: case CASTLEKING:
            change = ((v == KING) ? CHANGEKING : 0);
            if (c < 7 && (u * refJeu [l][c+1] <= 0))
               nList = pushMove (listMove, u, change, nList, l, c, l, c+1);
            if (c > 0 && (u * refJeu [l][c-1] <= 0))
               nList = pushMove (listMove, u, change, nList, l, c, l, c-1);
            if (l < 7 && (u * refJeu [l+1][c] <= 0))
               nList = pushMove (listMove, u, change, nList, l, c, l+1, c);
            if (l > 0 && (u * refJeu [l-1][c] <= 0))
               nList = pushMove (listMove, u, change, nList, l, c, l-1, c);
            if ((l < 7) && (c < 7) && (u * refJeu [l+1][c+1] <= 0))
               nList = pushMove (listMove, u, change, nList, l, c, l+1, c+1);
            if ((l < 7) && (c > 0) && (u * refJeu [l+1][c-1] <= 0))
               nList = pushMove (listMove, u, change, nList, l, c, l+1, c-1);
            if ((l > 0) && (c < 7) && (u * refJeu [l-1][c+1] <= 0))
               nList = pushMove (listMove, u, change, nList, l, c, l-1, c+1);
            if ((l > 0) && (c > 0) && (u * refJeu [l-1][c-1] <= 0))
               nList = pushMove (listMove, u, change, nList, l, c, l-1, c-1);
            break;

         case KNIGHT:
            if (l < 7 && c < 6 && (u * refJeu [l+1][c+2] <= 0)) 
               nList = pushMove (listMove, u, 0, nList, l, c, l+1, c+2);
            if (l < 7 && c > 1 && (u * refJeu [l+1][c-2] <= 0))
               nList = pushMove (listMove, u, 0, nList, l, c, l+1, c-2);
            if (l < 6 && c < 7 && (u * refJeu [l+2][c+1] <= 0))
               nList = pushMove (listMove, u, 0, nList, l, c, l+2, c+1);
            if (l < 6 && c > 0 && (u * refJeu [l+2][c-1] <= 0))
               nList = pushMove (listMove, u, 0, nList, l, c, l+2, c-1);
            if (l > 0 && c < 6 && (u * refJeu [l-1][c+2] <= 0))
               nList = pushMove (listMove, u, 0, nList, l, c, l-1, c+2);
            if (l > 0  && c > 1 && (u * refJeu [l-1][c-2] <= 0))
               nList = pushMove (listMove, u, 0, nList, l, c, l-1, c-2);
            if (l > 1 && c < 7 && (u * refJeu [l-2][c+1] <= 0))
               nList = pushMove (listMove, u, 0, nList, l, c, l-2, c+1);
            if (l > 1 && c > 0 && (u * refJeu [l-2][c-1] <= 0))
               nList = pushMove (listMove, u, 0, nList, l, c, l-2, c-1);
            break;

         case ROOK: case QUEEN:
            for (k = l+1; k < N; k++) {   // en haut
               if ((w = refJeu [k][c]) * u  <= 0) {
                  nList = pushMove (listMove, u, 0, nList, l, c, k, c);
                  if (w)  break;          // si w != 0...
               }
               else break;
            }
            for (k = l-1; k >=0; k--) {   // en bas
               if ((w = refJeu [k][c]) * u <= 0) {
                  nList = pushMove (listMove, u, 0, nList, l, c, k, c);
                  if (w) break;
               }
               else break;
            }
            for (k = c+1; k < N; k++) {   // a droite
               if ((w = refJeu [l][k]) * u <= 0) {
                  nList = pushMove (listMove, u, 0, nList, l, c, l, k);
                  if (w) break;
               }
               else break;
            }
            for (k = c-1; k >=0; k--)  {  // a gauche
               if ((w = refJeu [l][k]) * u <= 0) {
                  nList = pushMove (listMove, u, 0, nList, l, c, l, k);
                  if (w) break;
               }
               else break;
            }
            if (v == ROOK) break;
         // surtout pas de break pour la reine
         case BISHOP :                             // valable aussi pour QUEEN
            for (k = 0; k < MIN (7-l, 7-c); k++) { // vers haut droit
               if ((w = refJeu [l+k+1][c+k+1]) * u <= 0) {
                  nList = pushMove (listMove, u, 0, nList, l, c, l+k+1, c+k+1);
                  if (w) break;
               }
               else break;
            }
            for (k = 0; k < MIN (7-l, c); k++) {   // vers haut gauche
               if ((w = refJeu [l+k+1][c-k-1]) * u <= 0) {
                  nList = pushMove (listMove, u, 0, nList, l, c, l+k+1, c-k-1);
                  if (w) break;
               }
               else break;
            }
            for (k = 0; k < MIN (l, 7-c); k++) {   // vers bas droit
               if ((w = refJeu [l-k-1][c+k+1]) * u <= 0) {
                  nList = pushMove (listMove, u, 0, nList, l, c, l-k-1, c+k+1);
                  if (w) break;
               }
               else break;
            }
            for (k = 0; k < MIN (l, c); k++) {     // vers bas gauche
               if ((w = refJeu [l-k-1][c-k-1]) * u  <= 0) {
                  nList = pushMove (listMove, u, 0, nList, l, c, l-k-1, c-k-1);
                  if (w) break;
               }
               else break;
            }
            break;
         default:;
         } //fin du switch
      }    // fin du if
   }       // fin du for sur z
   return nList;
}

bool LCkingCannotMove (TGAME sq64, register int who, register int l, register int c) { /* */
   /* vrai si le roi du joueur "who" ne peut plus bouger sans se mettre echec au roi */
   /* "who" est la couleur du roi qui est attaque, l et c sa position */
   /* on essaye tous les jeux possibles. Si dans tous les cas on est echec au roi */
   /* c'est perdu. Noter que si le roi a le trait et qu'il n'est pas echec au roi il est Pat */
   /* si le roi est echec au roi il est mat */
   TLISTMOVE list;
   TGAME localSq64;
   register int z;
   register int maxList = buildList (sq64, who, true, true, list);
   if (maxList == 0) return true;
   for (register int k = 0; k < maxList; k++) {
      memcpy (localSq64, sq64, GAMESIZE);
      z = doMove0 (localSq64, list [k]); // si z != -1 z contient les coord. du roi qui a bouge z = 8*l +c
      if (! LCkingInCheck (localSq64, who, (z == -1) ? l : LINE (z), (z == -1) ? c : COL (z))) return false;
   }
   return true;
}

int evaluation (TGAME sq64, register int who, bool *pat) { /* */
   /* fonction d'evaluation retournant MAT si Ordinateur gagne, */
   /* -MAT si joueur gagne, 0 si nul,... */
   /* position le boolean pat si pat */
   register int l, c, v, z, zAdverse;
   register int8_t *p64 = &sq64 [0][0];
   register int lAdverse, cAdverse, nBBishops, nWBishops;
   bool kingInCheck;
   register int eval = 0;
   if (who == BLACK) {
      z = findBlackKing (sq64);
      zAdverse = findWhiteKing (sq64);
   }
   else {
      z = findWhiteKing (sq64);
      zAdverse = findBlackKing (sq64);
   }
   if (LCkingInCheck (sq64, who, LINE (z), COL (z))) return -who * (MATE+1); // who ne peut pas jouer et se mettre en echec
   kingInCheck = LCkingInCheck (sq64, -who, lAdverse = LINE (zAdverse), cAdverse = COL (zAdverse));
   if (LCkingCannotMove (sq64, -who, lAdverse, cAdverse)) { 
      if (kingInCheck) return who * MATE;
      else {
         *pat = true;
         return 0;   // Pat a distinguer de "0" avec le boolen pat
      }
   }
   if (kingInCheck) eval += who * KINGINCHECKEVAL;
   *pat = false;
   // ladverse = cadverse = 0;
   nBBishops = nWBishops = 0;  // nombre ce fous Black, White.
   info.nEvalCall += 1;
   for (register int z = 0; z < GAMESIZE; z++) {
      // eval des pieces
      if ((v = (*p64++)) == 0) continue;
      // v est la valeur courante
      l = LINE (z);
      c = COL (z);
      eval += ((v > 0) ? valPiece [v] : -valPiece [-v]);
      // bonus si cavalier fou tour reine dans le carre central
      if ((c == 3 || c == 4) && (l == 3 || l == 4)) {
         if (v > PAWN && v < KING) eval += BONUSCENTER;
         else if (v < -PAWN && v > -KING) eval -= BONUSCENTER;
      }
       
      switch (v) {
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
   return eval;
}

int alphaBeta (TGAME sq64, int who, int p, int refAlpha, int refBeta, uint64_t zobrist) { /* */
   /* le coeur du programme */
   info.nAlphaBeta += 1;
   TLISTMOVE list;
   TGAME localSq64;
   int maxList = 0;
   int k, note;
   bool pat = false;
   int val;
   int alpha = refAlpha;
   int beta = refBeta;
   uint64_t newZobrist = 0;
   uint32_t hash = 0, check = 0;
   bool end = false;
   // uint64_t zobrist = computeHash (sq64);
   if (getInfo.trans) {
      hash = zobrist & masqMaxTransTable;
      check = zobrist >> 32; 
      if (trTa [hash].used && (trTa [hash].p <= p)) { 
         if (trTa [hash].check == check) {
            info.nbMatchTrans += 1;
            return (trTa [hash].eval);
         }
         else info.nbColl += 1;    // détection de collisions
      }
   }
   note = evaluation (sq64, who, &pat);
   // conditions de fin de jeu
   if (note >= MATE) { end = true; note -= p; }   // -p pour favoriser le choix avec faible profondeur
   if (note <= -MATE) { end = true; note += p; }  // +p idem
   if (pat) { end = true; note = 0; }
   if (p >= info.maxDepth) {
      if (getInfo.trans) { 
         trTa [hash].eval = note;
         trTa [hash].p = p;
         trTa [hash].used = true;
         trTa [hash].check = check;
         info.nbTrTa += 1;
      }
      return note;
   }
   if (end) return note;
   
   // pire des notes a ameliorer
   if (who == 1) {
      val = MATE;
      maxList = buildList (sq64, -1, true, true, list);
      for (k = 0; k < maxList; k++) {
         memcpy (localSq64, sq64, GAMESIZE);
         if (getInfo.trans) newZobrist = doMove (localSq64, list [k], zobrist);
         else doMove0 (localSq64, list [k]);
         // if (newZobrist != computeHash (localSq64)) {printf ("ERR: Zobrist 1\n"); exit (0);};
         note = alphaBeta (localSq64, -1, p+1, alpha, beta, newZobrist);
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
         memcpy (localSq64, sq64, GAMESIZE);
         if (getInfo.trans) newZobrist = doMove (localSq64, list [k], zobrist);
         else doMove0 (localSq64, list [k]);
         // if (newZobrist != computeHash (localSq64)) {printf ("ERR: Zobrist 2\n"); exit (0);};
         note = alphaBeta (localSq64, 1, p+1, alpha, beta, newZobrist);
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
   info.moveList[k].eval = alphaBeta (info.moveList [k].jeu, -gamer.color, 0, -MATE, MATE, info.moveList [k].zobrist);
   pthread_exit (NULL);
}

int comp (const void *a, const void *b) {
   /*comparaison evaluation de deux move pour tri croissant si couleur gamer blanche, decroissante sinon */
   const TMOVEINFO *pa = a;
   const TMOVEINFO *pb = b;
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
   int bestNote = gamer.color * (MATE + 1); // la pire des notes possibleq
   int i;
   long k;
   TGAME localSq64;
   struct timeval tRef;
   int lGK, cGK, lCK, cCK; // ligne colonnes Gamer et Computer King 
   uint64_t zobrist = 0;
   masqMaxTransTable  = (1 << getInfo.exp) - 1; // idem (2 puissance exp) - 1
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
   
   // check etat du joueur
   if (LCkingInCheck (sq64, gamer.color, lGK, cGK)) {
      if (LCkingCannotMove(sq64, gamer.color, lGK, cGK)) gamer.kingState = ISMATE;
      else gamer.kingState = UNVALIDINCHECK;
      info.score = ERROR;
      return 0; // le joueur n'a pas le droit de se presenter en echec apres avoir joue
   }
   // check du computer
   if (LCkingInCheck(sq64, -gamer.color, lCK, cCK))
      computer.kingState = ISINCHECK;
   if (LCkingCannotMove(sq64, -gamer.color, lCK, cCK)) {
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
   gamer.nValidPos = buildList (sq64, gamer.color, gamer.kingCastleOK, gamer.queenCastleOK, listMove);
   nextL = buildList (sq64, -gamer.color, computer.kingCastleOK, computer.queenCastleOK, listMove);
   nextL = buildListEnPassant (sq64, -gamer.color, gamer.ep, listMove, nextL);
   computer.nValidPos = nextL;
   if (nextL == 0) {
      strcpy (info.comment, "ERR: no move. Strange");
      return 0; // impossible de jouer... Bizarre
   }
   for (k = 0; k < nextL; k++) { // enregistrement des move possibles au niveau 0 dans info
      memcpy (&info.moveList [k].move, &listMove [k], sizeof (TMOVE));
      info.moveList [k].taken = sq64 [listMove [k].l2][listMove [k].c2]; 
      moveToStr (info.moveList [k].move, info.moveList [k].strMove, info.moveList [k].taken);
   }
   
   info.maxDepth = fMaxDepth (getInfo.level, gamer.nValidPos, computer.nValidPos);

   // lancement de la recherche par l'ordi
   gameToFen (sq64, fen, -gamer.color, ' ', false, computer.ep, 0, 0);
   // recherche de fin de partie voir https://syzygy-tables.info/
   if ((info.nPieces) <= MAXPIECESSYZYGY) {
      sprintf (fen, "%s - - %d %d", fen, info.cpt50, info.nb); // pour regle des 50 coups
      if (syzygyRR (PATHTABLE, fen, &info.wdl, info.computerPlayC, info.comment)) {
         moveGame (sq64, -gamer.color, info.computerPlayC);
	      status = ENDGAME;
      }
   }
   else {
      // ouvertures
      if ((info.nb < MAXNBOPENINGS) &&
         (openingAll (OPENINGDIR, (gamer.color == WHITE) ? ".b.fen": ".w.fen", fen, info.comment, info.computerPlayC))) {
            moveGame (sq64, -gamer.color, info.computerPlayC);
            status = OUV;
         }
      }
   if (status == INIT) {
      if (getInfo.trans) {
         masqMaxTransTable  = (1 << getInfo.exp) - 1; // idem (2 puissance exp) - 1
         if ((trTa = calloc (masqMaxTransTable + 1, sizeof (StrTa))) == NULL) {
            strcpy (info.comment, "ERR: malloc in computerPlay ()\n");
            return 0;
         }
         initTable();                     // pour table hachage Zobrist
         zobrist = computeHash (sq64);
      }

      // recherche par la methode minimax Alphabeta
      if (getInfo.multi) { // Multi thread
         for (k = 0; k < nextL; k++) {
            memcpy (info.moveList [k].jeu, sq64, GAMESIZE);
            if (getInfo.trans) info.moveList [k].zobrist = doMove (info.moveList [k].jeu, info.moveList [k].move, zobrist);
            else doMove0 (info.moveList [k].jeu, info.moveList [k].move);
            // if (info.moveList [k].zobrist != computeHash (info.moveList [k].jeu)) {printf ("Error Zobrist \n")};
            if (pthread_create (&tThread [k], NULL, fThread, (void *) k)) {
               strcpy (info.comment, "ERR: pthread_create");
               return 0;
            }
         }
         for (k = 0; k < nextL; k++)
            if (pthread_join (tThread [k], NULL)) {
               strcpy (info.comment, "ERR: pthread_join");
               return 0;
            }
      }
      else {
         for (k = 0; k < nextL; k++) {
            memcpy (info.moveList [k].jeu, sq64, GAMESIZE);
            if (getInfo.trans) info.moveList [k].zobrist = doMove (info.moveList [k].jeu, info.moveList [k].move, zobrist);
            else doMove0 (info.moveList [k].jeu, info.moveList [k].move);
            // if (info.moveList [k].zobrist != computeHash (info.moveList [k].jeu)) {printf ("Error Zobrist\n");};
            info.moveList[k].eval = alphaBeta (info.moveList [k].jeu, -gamer.color, 0, -MATE, MATE, info.moveList [k].zobrist);
         }
      }
      // info.moveList contient les évaluations de toutes les possibilites
      // recherche de la meilleure note
      status = ALPHABETA;
      qsort (info.moveList, nextL, sizeof (TMOVEINFO), comp); // tri croisant ou decroissant selon couleur gamer
      info.evaluation = bestNote = info.moveList [0].eval;
      i = 0;
      while ((info.moveList[i].eval == bestNote) && (i < nextL)) i += 1; // il y a i meilleurs jeux
      info.nBestNote = i;
      switch (getInfo.alea) { 
         case -1 : k = i-1; break;
         case 0  : k = 0; break;
         default : k = rand () % i;
      }
      memcpy (sq64, info.moveList[k].jeu, GAMESIZE);
      moveToStr (info.moveList [k].move, info.computerPlayC, info.moveList [k].taken);
   }
   if (getInfo.trans) free (trTa);
  
   enPassant (-gamer.color, info.computerPlayC, computer.ep);
   abbrev (localSq64, info.computerPlayC, info.computerPlayA);
   
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
   if (LCkingCannotMove (sq64, gamer.color, lGK, cGK)) {
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
   flog = fopen (F_LOG, "a");                // preparation du fichier log 
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
   if ((str = strstr (env, "notrans")) != NULL) getInfo.trans = false;
   if ((str = strstr (env, "fen=")) != NULL)
      sscanf (str, "fen=%[a-zA-Z0-9+-/]", getInfo.fenString);
   if ((str = strstr (env, "level=")) != NULL)
      sscanf (str, "level=%d", &getInfo.level);
   if ((str = strstr (env, "reqType=")) != NULL)
      sscanf (str, "reqType=%d", &getInfo.reqType);
   if ((str = strstr (env, "exp=")) != NULL)
      sscanf (str, "exp=%d", &getInfo.exp);
   if ((str = strstr (env, "alea=")) != NULL)
      sscanf (str, "alea=%d", &getInfo.alea);
 
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
   TGAME localSq64;
   uint64_t hashValue; 
   // si pas de parametres on va au cgi (fin de main)
   if (argc >= 2 && argv [1][0] == '-') { // si il y a des parametres. On choidi un test
      if (argc > 2) strcpy (getInfo.fenString, argv [2]);
      if (argc > 3) getInfo.level = atoi (argv [3]);
      if (argc > 4) getInfo.exp = atoi (argv [4]);
      masqMaxTransTable  = (1 << getInfo.exp) - 1; // idem (2 puissance exp) - 1
      gamer.color = -fenToGame (getInfo.fenString, sq64, gamer.ep, &info.cpt50, &info.nb);
      switch (argv [1][1]) {
      case 'q': case 'v': // q quiet, v verbose
         printf ("argv1: %s\n", argv [1]);
         if (strlen (argv [1]) > 2) getInfo.trans = (argv [1][2] != 'n');
         if (strlen (argv [1]) > 3) getInfo.alea = (argv [1][3] == 'n') ? -1 : ((argv [1][3] == 'N') ? 0 : 1);
         if (strlen (argv [1]) > 4) getInfo.multi = (argv [1][4] != 'n');
         computerPlay ();
         if (argv [1][1] == 'v') 
            printGame (sq64, evaluation (sq64, -gamer.color, &info.pat));
         gameToFen (sq64, fen, gamer.color, '+', true, computer.ep, info.cpt50, info.nb); 
         sendGame (false, fen, getInfo.reqType);
         break;
      case 'f': //performance
         initTable ();
         hashValue = computeHash (sq64); 
         info.nClock = clock ();
         for (uint64_t i = 0; i < 2; i++) {
            memcpy (localSq64, sq64, GAMESIZE);
            sq64 [0][0] = KING;
            memcpy (sq64, localSq64, GAMESIZE);
         }         
         printf ("memcpy. clock: %ld\n", (clock () - info.nClock));
         printGame (localSq64, 0);
         
         info.nClock = clock ();
         int8_t *pl = &sq64[0][0]; 
         int8_t *pl2 = &localSq64[0][0]; 
         for (uint64_t i = 0; i < 2; i++) {
            asm volatile ("movl $8, %%ecx; rep movsq" : "+D" (pl2) : "S"(sq64) : "ecx", "cc", "memory");
            sq64 [0][0] = KING;
            asm volatile ("movl $8, %%ecx; rep movsq" : "+D" (pl) : "S"(localSq64) : "ecx", "cc", "memory");
         }         
         printf ("asmcpy. clock: %ld\n", (clock () - info.nClock));
         printGame (localSq64, 0);
         break;
      case 't': // tests
         printGame (sq64, evaluation (sq64, -gamer.color, &info.pat));
         break;
      case 'd': // display
         printGame (sq64, evaluation (sq64, gamer.color, &info.pat));
         gameToFen (sq64, fen, gamer.color, '+', true, computer.ep, info.cpt50, info.nb); 
         printf ("{ \"fen\" : \"%s\"}\n",fen);
         break; 
      case 'm': case 'M': // move
         moveGame (sq64, gamer.color, argv [argc-1]);        
         if (argv [1][1] == 'M') printGame (sq64, evaluation (sq64, gamer.color, &info.pat));
         gameToFen (sq64, fen, gamer.color, '+', true, computer.ep, info.cpt50, info.nb); 
         printf ("{ \"fen\" : \"%s\", ",fen);
         printf (" \"score\" : \"-\", ");
         printf (" \"time\" : 0}\n");
         break;
      case 'z': // Zobrist
         // Move the white king to the left 
         initTable(); 
         hashValue = computeHash (sq64); 
         printf ("The hash value is     : %lu\n", hashValue); 
         uint8_t piece = sq64[0][3]; 
         sq64 [0][3] = 0; 
         hashValue ^= ZobristTable[0][3][indexOf(piece)]; 
         sq64 [0][2] = piece; 
         hashValue ^= ZobristTable[0][2][indexOf(piece)]; 
         printf ("The new hash value is : %lu\n", hashValue); 
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
