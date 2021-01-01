#define _DEFAULT_SOURCE // pour scandir
#define NIL -9999999
#define MAXLENBIG 10000

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>
#include <dirent.h>

#include "chessUtil.h"
#include "vt100.h"
// Pawn, kNight, Bishop, Rook, Queen, King, rOckking
// FEN notation
// White : Majuscules, negatives. Black: Minuscules, positives. 

const char dict [] = {'-', 'P', 'N', 'B', 'R', 'Q', 'K', 'K'};
const char *unicode [] = {" ", "♟", "♞", "♝", "♜", "♛", "♚", "♚"};
const char *strStatus [] = {"NO_EXIST", "EXIST", "IS_IN_CHECK", "UNVALID_IN_CHECK", "IS_MATE", "IS_PAT"};

int charToInt (int c) { /* */
   /* traduit la piece au format RNBQR... en nombre entier */
   int sign = islower (c) ? 1 : -1;
   for (unsigned int i = 0; i < sizeof (dict); i++)
      if (toupper (c) == dict [i]) return sign * i;
   return NIL;
}

void printGame (TGAME jeu, int eval) { /* */
   /* imprime le jeu a la console pour Debug */
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

int fenToGame (char *fenComplete, TGAME sq64, char *ep, int *cpt50, int *nb) { /* */
   /* Forsyth–Edwards Notation */
   /* le jeu est recu sous la forme d'une chaine de caracteres du navigateur au format fen */
   /* fenToGame traduit cette chaine et renvoie l'objet jeu ainsi que la couleur */
   /* 3kq3/8/8/8/8/3K4/+w+-- */
   /* retour 1 si noir, -1 si blanc */
   /* le roque est contenu dans la valeur du roi : KING ou CASTLEKING */
   /* les valeurs : en passant, cpt 50 coups et nb de coups sont renvoyées */
   /* les separateurs acceptés entre les differents champs sont : + et Espace */ 
   int k, l = 7, c = 0;
   char *fen, cChar;
   char *sColor, *sCastle, *strNb, *str50, *strEp;
   char copyFen [MAXLEN];
   bool bCastleW = false;  
   bool bCastleB = false;
   int activeColor = 1; //par defaut : noir
   *cpt50 = *nb = 0;
   strcpy (copyFen, fenComplete);
   fen = strtok (copyFen, "+ ");
   if ((sColor = strtok (NULL, "+ ")) != NULL)        // couleur
      activeColor = (sColor [0] == 'b') ? 1 : -1;    
   if ((sCastle = strtok (NULL, "+ ")) != NULL) {     // roques
      bCastleW = (sCastle [0] == '-');
      bCastleB = (bCastleW ? sCastle [1] == '-' : sCastle [2] == '-');
   }
   if ((strEp = strtok (NULL, "+ ")) != NULL)         // en passant
      strcpy (ep, strEp);
   if ((str50 = strtok (NULL, "+ ")) != NULL)
      *cpt50 = atoi (str50);                         // pour regle des cinquante coups
   if ((strNb = strtok (NULL, "+ ")) != NULL)  
      *nb = atoi (strNb);                            // nbcoup
   
   for (unsigned i = 0; fen [i] != '\0'; i++) {
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
         if (cChar == 'k' && bCastleB) sq64 [l][c] = CASTLEKING;  // le roi noir a deja roque
         c += 1;
      }
      if (c == N) {
         l -= 1;
         c = 0;
      }
   }
   return activeColor;
}

char *gameToFen (TGAME sq64, char *fen, int color, char sep, bool complete, char *ep, int cpt50, int nb) { /* */
   /* Forsyth–Edwards Notation */
   /* genere le jeu sous la forme d'une chaine de caracteres au format FEN */
   /* le separateur est donne en parametre : normalement soit espace soit "+" */
   /* si le boolean "complete" est vrai vrai alors on transmet le roque, la valeur en passant, */
   /* le compteur des 50 coups et le nb de coups */
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
   fen [i] = '\0';
   sprintf (fen, "%s%c%c", fen, sep, (color == 1) ? 'b': 'w');
   if (complete)
      sprintf (fen, "%s%c%s%s%c%s%c%d%c%d", fen, sep, (castleW ? "-":"KQ"), (castleB ? "-":"kq"), sep, ep, sep, cpt50, sep, nb); 
   return fen;
}

void moveGame (TGAME sq64, int color, char *move) { /* */
   /* modifie jeu avec le deplacement move */
   /* move en notation algébique Pa2-a4 ou Pa2xc3 */
   int base = (color == -1) ? 0 : 7; // Roque non teste
   int cDep, lDep, cDest, lDest, i, j;
   
   if (strcmp (move, "O-O-O") == 0) {   // grand Roque
      sq64 [base][4] = VOID;
      sq64 [base][3] = ROOK * color;
      sq64 [base][2] = KING * color;
      sq64 [base][0] = VOID;
      return;
   }
   if (strcmp (move, "O-O") == 0) {     // petit Roque
      sq64 [base][4] = VOID;
      sq64 [base][5] = ROOK * color;
      sq64 [base][6] = KING * color;
      sq64 [base][7] = VOID;
      return;
   }
   i = isupper (move[0]) ? 1 : 0; // nom de piece optionnel. S'il existe c'est une Majuscule.
   // - ou x optionnel, : tolere
   j = ((move [i+2] == 'x') || (move [i+2] == '-') || (move [i+2] == ':')) ? (i + 3) : (i + 2); 
   cDep = move [i] - 'a';
   lDep = move [i+1] - '1'; 
   cDest = move [j] - 'a';
   lDest = move [j+1] - '1';
  
   // on regarde si promotion comme dans : Pf2-f1=Q
   sq64 [lDest][cDest] = (move [strlen (move)-2] == '=') ? -color*(charToInt (move[strlen (move)-1])) : sq64 [lDep][cDep];
   sq64 [lDep][cDep] = VOID;
}

bool openingAll (const char *dir, const char *filter, char *gameFen, char *sComment, char *move) {
   /* liste les fichier du reperdoire dir contenant la chaine filter */
   /* fichier dans l'ordre alphabetique */
   /* donc nommer les fichier les plus prioritaires en debut d'alphabet */
   /* appelle opening sur les fichiers listes jusqu a trouver */
   /* renvoie vrai si gameFen est trouvee dans l'un des fichiers faux sinon */

   struct dirent **namelist;
   int n;
   char fileName [MAXLEN];
   char comment [MAXLEN];
   n = scandir (dir, &namelist, 0, alphasort);
   if (n < 0) return false;
   for (int i = 0; i < n; i++) {
      if (strstr (namelist [i]->d_name, filter) != NULL) {
         sprintf (fileName, "%s/%s", dir, namelist [i]->d_name);
         if (opening (fileName, gameFen, comment, move)) {
            sprintf (sComment, "%s %s", namelist [i]->d_name, comment);
            return true;
         }
      }
      free (namelist [i]);
   }
   free (namelist);
   return false;
}

bool opening (const char *fileName, char *gameFen, char *sComment, char *move) { /* */
   /* lit le fichier des ouvertures et produit le jeu final */
   /* ce fichier est au forma CSV : FENstring ; dep ; commentaire */
   /* dep contient le deplacement en notation algebrique complete Xe2:e4[Y] | O-O | O-O-O */
   /* X : piece joue. Y : promotion,  O-O : petit roque,  O-O-O : grand roque */
   /* renseihne la chaine move (deplacement choisi) et le commentaire associe */
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
      if (((sFEN = strtok (line, ";")) != NULL) && 
           (strncmp (sFEN, gameFen, MIN(strlen (sFEN), lenGameFen)) == 0)) {
         if ((ptDep = strtok (NULL, ";")) != NULL) {
            ptComment = strtok (NULL, "\n");
            strcpy (sComment, (ptComment != NULL) ? ptComment : "");
         }
         strcpy (move, ptDep);
         return true;
      }
   }
   return false;
}

bool symetryV (TGAME sq64, int l1, int c1, int cDest) { /* */ 
   /* vraie si il y a une piece egale a l1, c1 dans le symetrique par rapport a la colonne cDest */
   int cSym = cDest + cDest - c1;
   return (cSym >= 0 && cSym < N) ? (sq64 [l1][c1] == sq64 [l1][cSym]) : false;
}

bool symetryH (TGAME sq64, int l1, int c1, int lDest) { /* */ 
   /* vraie si il y a une piece egale a l1, c1 dans le symetrique par rapport a la ligne lDest */
   int lSym = lDest + lDest - l1;
   return (lSym >= 0 && lSym < N) ? (sq64 [l1][c1] == sq64 [lSym][c1]): false;
}

char *abbrev (TGAME sq64, char *complete, char *abbr) { /* */ 
   /* transforme la specif algebriqe complete en abregee */
   char cCharPiece = complete [0];
   int c1 = complete [1] - 'a';
   int l1 = complete [2] - '1';
   int c2 = complete [4] - 'a';
   int l2 = complete [5] - '1';
   char prise = complete [3];
   int v = sq64 [l1][c1];
   char strEnd [5] = "";
   char spec [3] = "";       // pour notation algebrique abrégée
   if (strlen (complete) >= 7) {
     for (unsigned int i = 0; i <= strlen (complete) - 6; i++)
       strEnd [i] = complete [6 + i];
   }
   // calcul de la notation abregee
   switch (abs (v)) {                              
   case PAWN: 
      if ((prise == 'x') && (symetryV (sq64, l1, c1, c2))) // deux pions sym. prenant en c2 a partir ligne l1
         sprintf (spec, "%c", c1 + 'a');                   // on donne la colonne
      break;
   case KNIGHT:
      if (symetryV (sq64, l1, c1, c2)) 
         sprintf (spec, "%c", c1 + 'a');        //cavaliers sym. par rapport à la col. dest. on donne la col. 
      else if (symetryH (sq64, l1, c1, l2)) 
         sprintf (spec, "%c", l1 + '1');        //cavaliers sym. par rapport à la ligne dest. on donne la lig. 
      break;
      
   case ROOK:
      if ((l1 == l2) && (c1 < c2)) {            // meme ligne, recherche a droite  
         for (int i = (c2 + 1); i < N; i++) {
            if (sq64 [l1][i] == v) {            // il y a une autre tour capable d'aller vers l2 c2
               sprintf (spec, "%c", c1 + 'a');  // Trouve. on donne la colonne
               break;
            }
            if (sq64 [l2][i] != VOID) break;
         }
      }
      if ((l1 == l2) && (c1 > c2)) {            // meme ligne, recherche a droite  
         for (int i = (c2 - 1); i >= 0; i--) {
            if (sq64 [l1][i] == v) {            // il y a une autre tour capable d'aller vers l2 c2
               sprintf (spec, "%c", c1 + 'a');  // Trouve. On donne la colonne
               break;
            }
            if (sq64 [l2][i] != VOID) break;
         }
      }
      if ((c1 == c2) && (l1 < l2)) {            // meme colonne, recherche en bas 
         for (int i = (l2 + 1); i < N; i++) {
            if (sq64 [i][c1] == v) {            // il y a une autre tour capable d'aller vers l2 c2
               sprintf (spec, "%c", l1 + '1');
               break;
            }
            if (sq64 [i][c2] != VOID) break;
         }
      }
      if ((c1 == c2) && (l1 > l2)) {            // meme colonne, recherce en haut  
         for (int i = (l2 - 1); i >= 0; i--) {
            if (sq64 [i][c1] == v) {            // il y a une autre tour capable  d'aller vers l2 c2
               sprintf (spec, "%c", l1 + '1');
               break;
            }
            if (sq64 [i][c2] != VOID) break;
         }
      }
      break;
   case QUEEN:             // cas ou il y aurait 2 reines apres une promotion
      for (int l = 0; l < N; l++)
         for (int c = 0; c < N; c ++)
            if (l != l1 && c != c1 && sq64 [l][c] == v) {
               sprintf (spec, "%c%c", c1 + 'a', l1 + '1');
               break;
            }
      break;
   default:;               // BISHOP, KING
   }
   abbr [0] = (cCharPiece == 'P') ? '\0': cCharPiece; // premier caractere
   sprintf (abbr, "%s%s%s%c%d%s", abbr, spec, ((prise == 'x') ? "x" : ""), c2 + 'a', l2 + 1, strEnd);
   return abbr;
}


char *enPassant (int color, char *complete, char *strEp) { /* */
   /* renvoie les coordonnees eventuelles de la case en passant au format e5 */
   /* sinon renvoie "-" */
   int c1 = complete [1] - 'a';
   int l1 = complete [2] - '1';
   int c2 = complete [4] - 'a';
   int l2 = complete [5] - '1';
   if ((complete [0] == 'P') && (c2 == c1) && ((l2 - l1) == -color * 2))
      sprintf (strEp, "%c%c", c1 + 'a', (l1 - color) + '1'); 
   else sprintf (strEp, "-");
   return strEp;
}

char *difference (TGAME sq64_1, TGAME sq64_2, int color, char *prise, char *complete, char *abbr, char *epGamer, char *epComputer) { /* */
   /* color = 1 pour les noirs, - 1 pour les blancs */
   /* retrouve le coup joue par Ordi */
   /* traite le roque. En dehors de ce cas */
   /* traite le en passant suggere par le joueur */
   /* en dehors de ces deux cas suppose qu'il n'y a que deux cases differentes (l1,c1) (l2, c2) */
   /* renvoie la chaine decrivant la difference */
   /* qui represente le deplacement au format complet Pe2:e4 et au format abregé e4 */
   /* renvoie aussi prise est la valeur de la piece prise. ' ' si pas de prise */
   /* renvoie aussi epComputer - en passant - pour renseigner les coordonnees eventuelles de prise en passant (sinon : "-") */
   /* epGamer permet de prendre en compte l'indication du gamer pour une prise possible */
   int l1, c1, l2, c2, lCastling;
   int lEp, cEp;                    // pour prise en Passant
   char promotion [3] = "";
   char cCharPiece = ' ';
   int v = 0;
   *prise = '\0';
   sprintf (complete, "%s", "");
   l1 = c1 = l2 = c2 = NIL;
   lCastling = (color == -1) ? 0 : 7;
   if (sq64_1[lCastling][4] == color*KING && sq64_2[lCastling][4] == VOID && // roque gauche
      sq64_1[lCastling][0] == color*ROOK && sq64_2 [lCastling][0] == VOID) {
      sprintf (complete, "%s", "O-O-O");
      sprintf (abbr, "%s", "O-O-O");
      return complete;
   }
   if (sq64_1[lCastling][4] == color*KING && sq64_2[lCastling][4] == VOID && // roque droit
      sq64_1[lCastling][7] == color*ROOK && sq64_2 [lCastling][7] == VOID) {
      sprintf (complete, "%s", "O-O");
      sprintf (abbr, "%s", "O-O");
      return complete;
   }

   if (epGamer [0] != '-') {     // prise en passant par color
      lEp = epGamer [1] - '1';
      cEp = epGamer [0] - 'a';
      if ((cEp > 0) && (sq64_1 [lEp+color][cEp-1] == color * PAWN) &&      // vers droite
         (sq64_1[lEp][cEp] == VOID) && (sq64_1 [lEp+color][cEp] == -color * PAWN) &&
         (sq64_2 [lEp+color][cEp-1] == VOID) && 
         (sq64_2 [lEp][cEp] == color * PAWN) && (sq64_2 [lEp+color][cEp] == 0)) {
         *prise = (color == 1) ? dict [PAWN] : tolower (dict [PAWN]);      // on prend la couleur opposee
         sprintf (complete, "%c%c%d%c%c%d .e.p.", cCharPiece, cEp-1+'a', lEp+color+ 1 , 'x', cEp + 'a', lEp + 1);
         sprintf (abbr, "%c%c%c%d .e.p", cEp-1+'a', 'x', cEp + 'a', lEp + 1);
         return complete;
      }
      if ((cEp < N) && (sq64_1 [lEp+color][cEp+1] == color * PAWN) &&      // vers gauche
         (sq64_1 [lEp][cEp] == VOID) && (sq64_1 [lEp+color][cEp] == -color * PAWN) &&
         (sq64_2 [lEp+color][cEp+1] == VOID) && 
         (sq64_2 [lEp][cEp] == color * PAWN) && (sq64_2 [lEp+color][cEp] == 0)) {
         *prise = (color == 1) ? dict [PAWN] : tolower (dict [PAWN]);      // on prend la couleur opposee
         sprintf (complete, "%c%c%d%c%c%d .e.p.", cCharPiece, cEp+1+'a', lEp+color+ 1 , 'x', cEp + 'a', lEp + 1);
         sprintf (abbr, "%c%c%c%d .e.p", cEp+1+'a', 'x', cEp + 'a', lEp + 1);
         return complete;
      }
   }

   for (int l = 0; l < N; l++) {
      for (int c = 0; c < N; c++) {
         if (sq64_1 [l][c] * sq64_2 [l][c] < 0) {        // couleur opposee => prise par couleur coul
            v = abs (sq64_1 [l][c]);
            *prise = (color == 1) ? dict [v] : tolower (dict [v]);      // on prend la couleur opposee
            l2 = l;
            c2 = c;
         }
         else if (sq64_1 [l][c] == 0 && sq64_2 [l][c] * color > 0) {    // arrivee coul
            l2 = l;
            c2 = c;
         }
         else if (sq64_1 [l][c] * color > 0 && sq64_2 [l][c] == 0) {    // depart coul
            l1 = l;
            c1 = c;
         }
      }
   }
   if (l1 == NIL) {
      strcpy (complete, "NONE");
      strcpy (abbr, "NONE");
      return complete;
   }
   if ((l1 < N) && (l1 >=0) && (c1 < N) && (c1 >= 0)) 
      v = sq64_1 [l1][c1];
   if ((v >= -CASTLEKING) && (v <= CASTLEKING))
      cCharPiece = (v > 0) ? dict [v] : tolower(dict [-v]);
   else cCharPiece = '?';
   cCharPiece = toupper (cCharPiece);
   if (((sq64_1 [l1][c1] == PAWN) && (sq64_2 [l2][c2] > PAWN) && (l2 == 0)) || 
       ((sq64_1 [l1][c1] == -PAWN) && (sq64_2 [l2][c2] < -PAWN) && (l2 == 7)))
      sprintf (promotion, "=%c", dict [abs (sq64_2 [l2][c2])]);

   sprintf (complete, "%c%c%d%c%c%d%s", cCharPiece, c1 + 'a', l1 + 1, 
         ((*prise != '\0') ? 'x' : '-'), c2 + 'a', l2 + 1, promotion);
   abbrev (sq64_1, complete, abbr);
   enPassant (color, complete, epComputer);
   return complete;
}

void sendGame (const char *fen, struct sinfo info, int reqType) { /* */
   /* envoie le jeu decrit par fen et info au format JSON */
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
      printf ("\"computerStatus\" : \"%d : %s\",\n", info.computerKingState, strStatus [info.computerKingState]);
      printf ("\"playerStatus\" : \"%d : %s\",\n", info.gamerKingState, strStatus [info.gamerKingState]);
      printf ("\"fen\" : \"%s\",\n", fen);
      if (info.lastCapturedByComputer >= ' ' && info.lastCapturedByComputer <= 'z')
         printf ("\"lastTake\" : \"%c\",\n", info.lastCapturedByComputer);
      else printf ("\"lastTake\" : \"%c\",\n", ' ');
      printf ("\"openingName\" : \"%s\",\n", info.comment);
      printf ("\"endName\" : \"%s\",\n", info.endName);
      printf ("\"wdl\" : \"%u\",\n", info.wdl);
      printf ("\"computePlayC\" : \"%s\",\n", info.computerPlayC);
      printf ("\"computePlayA\" : \"%s\"", info.computerPlayA);
   }
   if (reqType > 1) {
      printf (",\n\"dump\" : \"");
      printf ("  maxDepth=%d  nEvalCall=%d  nLCKingInCheck=%d", info.maxDepth, 
         info.nEvalCall, info.nLCKingInCheckCall);
      printf ("  nBuildList=%d  nValidComputerPos=%d", info.nBuildListCall, info.nValidComputerPos);
      printf ("  nValidPlayerPos=%d\"", info.nValidGamerPos);
   }
   printf ("\n}\n");
}
