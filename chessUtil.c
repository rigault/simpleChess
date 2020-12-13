#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>

#define NIL -9999999
#define MAXLENBIG 10000
#define RED "\033[1;31m"
#define DEFAULT_COLOR "\033[0;m"
#include "chessUtil.h"
// Pawn, kNight, Bishop, Rook, Queen, King, rOckking
// FEN notation
// White : Majuscules. Black: Minuscules 

const char dict [] = {'-', 'P', 'N', 'B', 'R', 'Q', 'K', 'K'};
const char *unicode [] = {"-", "\x26\5F", "\x26\5E", "\x26\5D", "\x26\5C", "\x26\5B", "\x26\5A", "\x26\5A"};

int charToInt (int c) { /* */
   /* traduit la piece au format RNBQR... en nombre entier */
   int sign = islower (c) ? 1 : -1;
   for (unsigned int i = 0; i < sizeof (dict); i++)
      if (toupper (c) == dict [i]) return sign * i;
   return NIL;
}

void printGame (TGAME jeu, int eval) { /* */
   /* imprime le jeu a la conole pour Debug */
   int l, c;
   int v;
   printf ("--------------------Eval: %d\n", eval);
   for (l = 7; l >= 0; l--) {
      for (c = 0; c < N; c++) {
         v = jeu [l][c];
         printf ("%s",  (v > 0) ? RED : DEFAULT_COLOR);
         printf ("%3c",  (v > 0) ? tolower(dict [v]): dict [-v]);
         printf ("%s", DEFAULT_COLOR);
      }
      printf ("\n");
   }
   printf ("-------------------------\n");
}

void fenToGame (char *fenComplete, TGAME sq64, char *activeColor) { /* */
   /* Forsyth–Edwards Notation */
   /* le jeu est recu sous la forme d'une chaine de cCharacteres du navigateur */
   /* FENToJeu traduit cette chaine et renvoie l'objet jeu ansi que la couleur */
   /* 3kq3/8/8/8/8/3K4/+w+-- */
   int k, l = 7, c = 0;
   char *fen, cChar;
   char *sCouleur, *sCastle;
   char copyFen [MAXLEN];
   bool bCastleW = false;  
   bool bCastleB = false;
   strcpy (copyFen, fenComplete);
   fen = strtok (copyFen, "+");
   if ((sCouleur = strtok (NULL, "+")) != NULL)
      *activeColor = sCouleur [0]; 
   if ((sCastle = strtok (NULL, "+")) != NULL) {
      bCastleW = (sCastle [0] == '-');
      bCastleB = (bCastleW ? sCastle [1] == '-' : sCastle [2] == '-');
   }
   for (unsigned i = 0; i < strlen (fen) ; i++) {
      cChar = fen [i];
      if (isspace (cChar)) break;
      if (cChar == '/') continue;
      if (isdigit (cChar)) {
         for (k = 0; k < cChar - '0'; k++) {
            sq64 [l][c] = VOID;
            c += 1;
         }
      }
      else {
         sq64 [l][c] = charToInt (cChar);
         if (cChar == 'K' && bCastleW) sq64 [l][c] = -CASTLEKING; // le roi blanc a deja roque
         if (cChar == 'k' && bCastleB) sq64 [l][c] = CASTLEKING; // le roi noir a deja roque
         c += 1;
      }
      if (c == N) {
         l -= 1;
         c = 0;
      }
   }
}

void gameToFen (TGAME sq64, char *fen, int color, char sep, bool complete) { /* */
   /* Forsyth–Edwards Notation */
   /* le jeu est envoye sous la forme d'une chaine de cCharacteres au format FEN au navigateur */
   int n, v;
   int i = 0;
   bool castleW = false;
   bool castleB = false;
   for (int l = N-1; l >=  0; l--) {
      for (int c = 0; c < N; c++) {
         if ((v = sq64 [l][c]) != VOID) {
            if (v == CASTLEKING) castleB = true;
            if (v == -CASTLEKING) castleW = true;
            fen [i++] = (v >= 0) ? tolower (dict [v]) : dict [-v];
         }
         else {
            for (n = 0; (c+n < N) && (sq64 [l][c+n] == VOID); n++);
            fen [i++] = '0' + n;
            c += n - 1;
         }
      }
      fen [i++] = '/';
   }
   i -= 1;
   fen [i++] = sep;
   fen [i++] = (color == 1) ? 'b': 'w';
   if (complete) {
      fen [i++] = sep; 
      fen [i++] = '\0'; 
      strcat (fen, (castleW ? "-" : "KQ"));
      strcat (fen, (castleB ? "-" : "kq"));
   }
   else fen [i] = '\0';
}

void moveGame (TGAME sq64, int color, char *move) { /* */
   /* modifie jeu avec le deplacement move */
   /* move en notation algébique Pa2-a4 ou Pa2xc3 */
   /* non teste pour les blancs. Marche pour les noirs */
   int base = (color == -1) ? 0 : 7; // Roque non teste
   int cDep, lDep, cDest, lDest, i, j;
   
   if (strcmp (move, "0-0") == 0) { // petit Roque
      sq64 [base][4] = VOID;
      sq64 [base][5] = ROOK * color;
      sq64 [base][6] = KING * color;
      sq64 [base][7] = VOID;
      return;
   }
   if (strcmp (move, "0-0-0") == 0) { // grand Roque
      sq64 [base][4] = VOID;
      sq64 [base][3] = ROOK * color;
      sq64 [base][2] = KING * color;
      sq64 [base][0] = VOID;
      return;
   }
   i = isupper (move[0]) ? 1 : 0; // nom de piece optionnel. S'il existe c'est une Majuscule.
   j = ((move [i+2] == 'x') || (move [i+2] == '-') || (move [i+2] == ':')) ? (i + 3) : (i + 2); // - ou x optionnel
   cDep = move [i] - 'a';
   lDep = move [i+1] - '0' - 1; 
   cDest = move [j] - 'a';
   lDest = move [j+1] - '0' - 1;
  
   // on regarde si promotion comme dans : Pf2-f1=Q
   sq64 [lDest][cDest] = (move [strlen (move) - 2] == '=') ? -color * (charToInt (move [strlen (move) - 1])) : sq64 [lDep][cDep];
   sq64 [lDep][cDep] = VOID;
}

bool opening (const char *fileName, char *gameFen, char *sComment, char *move) { /* */
   /* lit le fichier des ouvertures et produit le jeu final */
   /* ce fichier est au forma CSV : FENstring ; dep ; commentaire */
   /* dep contient le deplacement en notation algebrique complete Xe2:e4[Y] | 0-0 | 0-0-0 */
   /* X : piece joue. Y : promotion,  0-0 : petit roque,  0-0-0 : grand roque */
   /* nom Ouverture contient le nom trouve */
   /* renvoie vrai si ouverture trouvee dans le fichier, faux sinon */
   FILE *fe;
   char line [MAXLEN];
   char *ptComment = sComment;
   char *ptDep = move;
   char *sFEN;
   unsigned int lenGameFen = strlen (gameFen);
   if ((fe = fopen (fileName, "r")) == NULL) return false;
   while (fgets (line, MAXLEN, fe) != NULL) {
      // les deux fen string matchent si l'un commence par le debut de l'autre. MIN des longueurs.
      if (((sFEN = strtok (line, ";")) != NULL) && (strncmp (sFEN, gameFen, MIN(strlen (sFEN), lenGameFen)) == 0)) {
         if ((ptDep = strtok (NULL, ";")) != NULL) {
            ptComment = strtok (NULL, "\n");
         }
         strcpy (move, ptDep);
         strcpy (sComment, ptComment);
         return true;
      }
   }
   return false;
}

char *difference (TGAME sq64_1, TGAME sq64_2, int color, char *prise, char* temp) { /* */
   /* coul = 1 (ordi) si le joueur a les blancs */
   /* retrouve le coup joue par Ordi */
   /* traite le roque. En dehors de ce cas */
   /* suppose qu'il n' y a deux cases differentes (l1,c1) (l2, c2) */
   /* renvoie la chaine decrivant la difference */
   /* qui represente le deplacement au format a1:a1 */
   /* prise est la valeur de la piece prise toujour en majuscule. ' ' si pas de prise */
   int l1, c1, l2, c2, lCastling;
   char promotion [3] = "";
   char cCharPiece = ' ';
   int v = 0;
   *prise = ' ';
   sprintf (temp, "%s", "");
   l1 = c1 = l2 = c2 = NIL;
   lCastling = (color == -1) ? 0 : 7;
   if (sq64_1[lCastling][4] == color*KING && sq64_2[lCastling][4] == VOID && 
      sq64_1[lCastling][0] == color*ROOK && sq64_2 [lCastling][0] == VOID) {
      sprintf (temp, "%s", "0-0-0");
      return temp;
   }
   if (sq64_1[lCastling][4] == color*KING && sq64_2[lCastling][4] == VOID && 
      sq64_1[lCastling][7] == color*ROOK && sq64_2 [lCastling][7] == VOID) {
      sprintf (temp, "%s", "0-0");
      return temp;
   }
   for (int l = 0; l < N; l++) {
      for (int c = 0; c < N; c++) {
         if (sq64_1 [l][c] * sq64_2 [l][c] < 0) { // couleur opposee => prise par couleur coul
            *prise = dict [abs(sq64_1 [l][c])];
            l2 = l;
            c2 = c;
         }
         else if (sq64_1 [l][c] == 0 && sq64_2 [l][c] * color > 0) { // arrivee coul
            l2 = l;
            c2 = c;
         }
         else if (sq64_1 [l][c] * color > 0 && sq64_2 [l][c] == 0) { // depart coul
            l1 = l;
            c1 = c;
         }
      }
   }
   if ((l1 < N) && (l1 >=0) && (c1 < N) && (c1 >= 0)) 
      v = sq64_1 [l1][c1];
   if ((v >= -CASTLEKING) && (v <= CASTLEKING))
      cCharPiece = (v > 0) ? dict [v] : tolower(dict [-v]);
   else cCharPiece = '?';
   cCharPiece = toupper (cCharPiece);
   if (color == 1) {
      if (((sq64_1 [l1][c1] == PAWN) && (l2 == 0)) || 
         (sq64_1 [l1][c1] == -PAWN && (l2 == 7)))
         sprintf (promotion, "=%c", dict [abs (sq64_2 [l2][c2])]);
   }
   // promotion non implementee pour les noirs : Bug 
   sprintf (temp, "%c%c%d%c%c%d%s", cCharPiece, c1 + 'a', l1 + 1, ((*prise != ' ') ? 'x' : '-'), c2 + 'a', l2 + 1, promotion);
   return temp;
}

void sendGame (TGAME sq64, struct sinfo info, int reqType) { /* */
   /* envoie le jeu au format JSON */
   char fen [MAXLEN];
   char dump [MAXLENBIG];
   char temp [MAXLENBIG];
   info.nClock = clock () - info.nClock;
   printf ("Access-Control-Allow-Origin: *\n");
   printf ("Content-Type: text/html\n\n");
   printf ("{\n");
   printf ("\"description\" : \"%s\",\n", DESCRIPTION);
   printf ("\"compilation-date\": \"%s\",\n", __DATE__);
   printf ("\"version\" : \"%s\"", VERSION);
   if (reqType > 0) {
      gameToFen (sq64, fen, info.gamerColor, '+', true);
      printf (",\n");
      printf ("\"clockTime\": \"%lf\",\n", (double) info.nClock/CLOCKS_PER_SEC);
      printf ("\"time\" : \"%d\",\n", info.computeTime);
      printf ("\"note\" : \"%d\",\n", info.note);
      printf ("\"eval\" : \"%d\",\n", info.evaluation);
      printf ("\"computerStatus\" : \"%d\",\n", info.computerKingState);
      printf ("\"playerStatus\" : \"%d\",\n", info.gamerKingState);
      printf ("\"fen\" : \"%s\",\n", fen);
      printf ("\"lastTake\" : \"%c\",\n", info.lastCapturedByComputer);
      printf ("\"openingName\" : \"%s\",\n", info.comment);
      printf ("\"endName\" : \"%s\",\n", info.endName);
      printf ("\"wdl\" : \"%u\",\n", info.wdl);
      printf ("\"computePlay\" : \"%s\"", info.computerPlay);
   }
   if (reqType > 1) {
      sprintf (dump, "maxDepth=%d", info.maxDepth);
      sprintf (temp, "   nEval=%d", info.nEvalCall);
      strcat (dump, temp);
      sprintf (temp, "   nLCKingInCheck=%d", info.nLCKingInCheckCall);
      strcat (dump, temp);
      sprintf (temp, "   nBuildList=%d", info.nBuildListCall);
      strcat (dump, temp);
      sprintf (temp, "   nValidComputerPos=%d", info.nValidComputerPos);
      strcat (dump, temp);
      sprintf (temp, "   nValidPlayerPos=%d", info.nValidGamerPos);
      strcat (dump, temp);
      printf (",\n\"dump\" : \"%s\"", dump);
   }
   printf ("\n}\n");
}
