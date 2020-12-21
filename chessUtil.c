#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>
#include <dirent.h>

#define NIL -9999999
#define MAXLENBIG 10000
#include "chessUtil.h"
#include "vt100.h"
// Pawn, kNight, Bishop, Rook, Queen, King, rOckking
// FEN notation
// White : Majuscules, negatives. Black: Minuscules, positives. 

const char dict [] = {'-', 'P', 'N', 'B', 'R', 'Q', 'K', 'K'};
const char *unicode [] = {" ", "♟", "♞", "♝", "♜", "♛", "♚", "♚"};

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
   bool normal = true;
   for (int c = 'a'; c <= 'h'; c++) printf (" %c ", c);
   printf ("   --> eval: %d\n", eval);
   for (l = 7; l >= 0; l--) {
      for (c = 0; c < N; c++) {
         printf ("%s", (normal ? BG_CYAN : BG_BLACK));
         normal =! normal; 
         v = jeu [l][c];
         printf ("%s %s %s",  (v > 0) ? C_RED : C_WHITE, unicode [abs (v)], DEFAULT_COLOR);
      }
      printf ("  %d\n", l+1);
      normal =! normal; 
   }
   printf ("%s\n", NORMAL);
}

int fenToGame (char *fenComplete, TGAME sq64, int *cpt50, int *nb) { /* */
   /* Forsyth–Edwards Notation */
   /* le jeu est recu sous la forme d'une chaine de caracteres du navigateur au format fen*/
   /* FENToJeu traduit cette chaine et renvoie l'objet jeu ainsi que la couleur */
   /* 3kq3/8/8/8/8/3K4/+w+-- */
   /* retour 1 si noir -1 si blanc */
   /* le roque est contenu dans la valeur u roi : ROI ou ROIROQUE */
   int k, l = 7, c = 0;
   char *fen, cChar;
   char *sColor, *sCastle, *strNb, *str50;
   char copyFen [MAXLEN];
   bool bCastleW = false;  
   bool bCastleB = false;
   int activeColor = 1; //par defaut : noir
   *cpt50 = *nb = 0;
   strcpy (copyFen, fenComplete);
   for (unsigned i = 0; i < strlen (copyFen); i++)   // normalisation
      if (isspace (copyFen [i])) copyFen [i] = '+'; 
   fen = strtok (copyFen, "+");
   if ((sColor = strtok (NULL, "+")) != NULL)        // couleur
      activeColor = (sColor [0] == 'b') ? 1 : -1;    
   if ((sCastle = strtok (NULL, "+")) != NULL) {     // roques
      bCastleW = (sCastle [0] == '-');
      bCastleB = (bCastleW ? sCastle [1] == '-' : sCastle [2] == '-');
   }
   if ((strtok (NULL, "+")) != NULL) {};             // en passant non traite
   if ((str50 = strtok (NULL, "+")) != NULL)
      *cpt50 = atoi (str50);                         // pour regle des cinquante coups
   if ((strNb = strtok (NULL, "+")) != NULL)  
      *nb = atoi (strNb);                            // nbcoup
   
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
   return activeColor;
}

char *gameToFen (TGAME sq64, char *fen, int color, char sep, bool complete, int cpt50, int nb) { /* */
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
      sprintf (fen, "%s%c-%c%d%c%d", fen, sep, sep, cpt50, sep, nb); 
   }
   else fen [i] = '\0';
   return fen;
}

void moveGame (TGAME sq64, int color, char *move) { /* */
   /* modifie jeu avec le deplacement move */
   /* move en notation algébique Pa2-a4 ou Pa2xc3 */
   /* non teste pour les blancs. Marche pour les noirs */
   int base = (color == -1) ? 0 : 7; // Roque non teste
   int cDep, lDep, cDest, lDest, i, j;
   
   if ((strcmp (move, "O-O-O") == 0) || (strcmp (move, "0-0-0") == 0)) { // grand Roque
      sq64 [base][4] = VOID;
      sq64 [base][3] = ROOK * color;
      sq64 [base][2] = KING * color;
      sq64 [base][0] = VOID;
      return;
   }
   if ((strcmp (move, "O-O") == 0) || (strcmp (move, "0-0") == 0)) { // petit Roque
      sq64 [base][4] = VOID;
      sq64 [base][5] = ROOK * color;
      sq64 [base][6] = KING * color;
      sq64 [base][7] = VOID;
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

bool openingAll (const char *dir, const char *filter, char *gameFen, char *sComment, char *move) {
   /* liste les fichier du reperdoire dir contenant la chaine filter */
   /* appelle opening sur les fichiers listes jusqu a trouver */
   /* renvoie vraie si gameFen est trouvee dans l'un des fichiers faux sinon */
   struct dirent *lecture;
   DIR *rep;
   rep = opendir(dir);
   char fileName [MAXLEN];
   char comment [MAXLEN];
   while ((lecture = readdir(rep))) {
      if (strstr (lecture->d_name, filter) != NULL) {
         // printf ("%s\n", lecture->d_name);
         sprintf (fileName, "%s/%s", dir, lecture->d_name);
         if (opening (fileName, gameFen, comment, move)) {
            sprintf (sComment, "%s %s", lecture->d_name, comment);
            return true;
         }
      }
   }
   closedir(rep);
   return false;
}

bool opening (const char *fileName, char *gameFen, char *sComment, char *move) { /* */
   /* lit le fichier des ouvertures et produit le jeu final */
   /* ce fichier est au forma CSV : FENstring ; dep ; commentaire */
   /* dep contient le deplacement en notation algebrique complete Xe2:e4[Y] | O-O | O-O-O */
   /* X : piece joue. Y : promotion,  O-O : petit roque,  O-O-O : grand roque */
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
            strcpy (sComment, ptComment);
         }
         strcpy (move, ptDep);
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
      sprintf (temp, "%s", "O-O-O");
      return temp;
   }
   if (sq64_1[lCastling][4] == color*KING && sq64_2[lCastling][4] == VOID && 
      sq64_1[lCastling][7] == color*ROOK && sq64_2 [lCastling][7] == VOID) {
      sprintf (temp, "%s", "O-O");
      return temp;
   }
   for (int l = 0; l < N; l++) {
      for (int c = 0; c < N; c++) {
         if (sq64_1 [l][c] * sq64_2 [l][c] < 0) { // couleur opposee => prise par couleur coul
            v = abs (sq64_1 [l][c]);
            *prise = (color == 1) ? dict [v] : tolower (dict [v]); // on prend la couleur opposee
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

void sendGame (const char *fen, struct sinfo info, int reqType) { /* */
   /* envoie le jeu au format JSON */
   char dump [MAXLENBIG];
   char temp [MAXLENBIG];
   printf ("Access-Control-Allow-Origin: *\n");
   printf ("Content-Type: text/html\n\n");
   printf ("{\n");
   printf ("\"description\" : \"%s\",\n", DESCRIPTION);
   printf ("\"compilation-date\": \"%s\",\n", __DATE__);
   printf ("\"version\" : \"%s\"", VERSION);
   if (reqType > 0) {
      printf (",\n");
      printf ("\"clockTime\": \"%lf\",\n", (double) info.nClock/CLOCKS_PER_SEC);
      printf ("\"time\" : \"%lf\",\n", (double) info.computeTime/MILLION);
      printf ("\"note\" : \"%d\",\n", info.note);
      printf ("\"eval\" : \"%d\",\n", info.evaluation);
      printf ("\"computerStatus\" : \"%d\",\n", info.computerKingState);
      printf ("\"playerStatus\" : \"%d\",\n", info.gamerKingState);
      printf ("\"fen\" : \"%s\",\n", fen);
      if (info.lastCapturedByComputer >= ' ' && info.lastCapturedByComputer <= 'z')
         printf ("\"lastTake\" : \"%c\",\n", info.lastCapturedByComputer);
      else printf ("\"lastTake\" : \"%c\",\n", ' ');
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
