#define _DEFAULT_SOURCE // pour scandir
#include <stdint.h>
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
// White : Majuscules, negatives. Black: Minuscules, positives. 

static const char dict [] = {'-', 'P', 'N', 'B', 'R', 'Q', 'K', 'K'};
static const char *unicode [] = {" ", "♟", "♞", "♝", "♜", "♛", "♚", "♚"};
static const char *strStatus [] = {"NO_EXIST", "EXIST", "IS_IN_CHECK", "UNVALID_IN_CHECK", "IS_MATE", "IS_PAT"};
static const char *scoreToStr [] = {"ERROR", "-", "0-1","1/2-1/2","1-0"};

int charToInt (int c) { /* */
   /* traduit la piece au format RNBQK... en nombre entier */
   int sign = islower (c) ? 1 : -1;
   for (unsigned int i = 0; i < sizeof (dict); i++)
      if (toupper (c) == dict [i]) return sign * i;
   return 0;
}

void moveToStr (TMOVE move, char str [MAXSTRMOVE]) { /* */
   /* traduit move en chaine algébrique */
   switch (move.type) {
   case STD: case PROMOTION: case ENPASSANT:
      str [0] = (move.who == 0) ? '?' : dict [abs(move.who)];
      str [1] = move.c1 + 'a';
      str [2] = move.l1 + '1';
      str [3] = (move.taken == 0) ? '-' : 'x';
      str [4] = move.c2 + 'a';
      str [5] = move.l2 + '1';
      str [6] = '\0';
      if (move.type == PROMOTION) {
         strcat (str, "=Q");
         str [0] = 'P';
      }
      if (move.type == ENPASSANT) {
         strcat (str, ".e.p");
      }
      break;
   case QUEENCASTLESIDE:
      strcpy (str, "O-O-O ");
      break; 
   case KINGCASTLESIDE:
      strcpy (str, "O-O   ");
      break; 
   default:;
   }
}

void printGame (TGAME jeu, int eval) { /* */
   /* imprime le jeu a la console pour Debug */
   int l, c;
   int v;
   bool normal = true;
   for (int c = 'a'; c <= 'h'; c++) printf (" %c ", c);
   printf ("   --> : %d\n", eval);
   for (l = 7; l >= 0; l--) {
      for (c = 0; c < N; c++) {
         printf ("%s", (normal ? BG_CYAN : BG_BLACK));
         normal =! normal; 
         v = jeu [l][c];
         if ((v < -CASTLEKING) || v > CASTLEKING) printf ("Error: l c v = %d %d %d\n ", l, c, v); 
         else printf ("%s %s %s",  (v > 0) ? C_RED : C_WHITE, unicode [abs (v)], DEFAULT_COLOR);
      }
      printf ("  %d\n", l+1);
      normal =! normal; 
   }
   printf ("%s\n", NORMAL);
}

char *castleToStr (bool whiteIsCastled, bool blackIsCastled, char *str) { /* */
   /* traduit booleen decrivant les possibilites de roque en string */
   /* modifie : gamer et computer */
   /* renvoie str de la forme KQkq */
   strcpy (str, "");

   if (((whiteIsCastled) && (gamer.color == WHITE)) || ((blackIsCastled) && (gamer.color == BLACK))) { 
      gamer.kingCastleOK = gamer.queenCastleOK = false;
   }
   if (((whiteIsCastled) && (gamer.color == BLACK)) || ((blackIsCastled) && (gamer.color == WHITE))) {
      computer.kingCastleOK = computer.queenCastleOK = false;
   }

   if (gamer.color == WHITE) {
      if (gamer.kingCastleOK) strcat (str, "K");
      if (gamer.queenCastleOK) strcat (str, "Q");
      if (computer.kingCastleOK) strcat (str, "k");
      if (computer.queenCastleOK) strcat (str, "q");
   }
   else {
      if (computer.kingCastleOK) strcat (str, "K");
      if (computer.queenCastleOK) strcat (str, "Q");
      if (gamer.kingCastleOK) strcat (str, "k");
      if (gamer.queenCastleOK) strcat (str, "q");
   }
   if (strlen (str) == 0) strcpy (str, "-");
   return str;
}

void strToCastle (char *str, int color, bool *whiteCanCastle, bool *blackCanCastle) { /* */
   /* traduit les possibilites de roque en booleens */
   /* modifie : gamer et computer */
   char car;
   *whiteCanCastle = false;
   *blackCanCastle = false;
   gamer.kingCastleOK = computer.kingCastleOK = gamer.queenCastleOK = computer.queenCastleOK = false;
   while ((car = *str++) != '\0') {
      switch (car) {
      case 'K': if (color == BLACK) gamer.kingCastleOK = true;
                else computer.kingCastleOK = true;
                *whiteCanCastle = true;
                break;
      case 'k': if (color == WHITE) gamer.kingCastleOK = true;
                else computer.kingCastleOK = true;
                *blackCanCastle = true;
                break;
      case 'Q': if (color == BLACK) gamer.queenCastleOK = true;
                else computer.queenCastleOK = true;
                *whiteCanCastle = true;
                break;
      case 'q': if (color == WHITE) gamer.queenCastleOK = true;
                else computer.queenCastleOK = true;
                *blackCanCastle = true;
                break;
      default:;
      }
   }
}

int fenToGame (char *fenComplete, TGAME sq64, char *ep, int *cpt50, int *nb) { /* */
   /* Forsyth–Edwards Notation */
   /* ex : 3kq3/8/8/8/8/3K4/+w+-- */
   /* le jeu est recu sous la forme d'une chaine de caracteres du navigateur au format fen */
   /* fenToGame traduit cette chaine et renvoie l'objet TGAME sq64  ainsi que la couleur */
   /* retour 1 si noir, -1 si blanc */
   /* le roque est contenu dans la valeur du roi : KING ou CASTLEKING */
   /* les valeurs : en passant, cpt 50 coups et nb de coups sont renvoyées */
   /* les separateurs acceptés entre les differents champs sont : + et Espace */ 
   int k, l = 7, c = 0;
   char *fen, cChar;
   char *sColor, *sCastle, *strNb, *str50, *strEp;
   char copyFen [MAXBUFFER];
   bool bCastleW = false; // defaut le roi blanc ne peut pas roquer  
   bool bCastleB = false;
   int activeColor = BLACK; //par defaut : noir
   *cpt50 = *nb = 0;
   strcpy (copyFen, fenComplete);
   fen = strtok (copyFen, "+ ");
   if ((sColor = strtok (NULL, "+ ")) != NULL)        // couleur
      activeColor = (sColor [0] == 'b') ? BLACK : WHITE;    
   if ((sCastle = strtok (NULL, "+ ")) != NULL) {     // roques
      strToCastle (sCastle, activeColor, &bCastleW, &bCastleB);
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
            sq64 [l][c] = 0;
            c += 1;
         }
      }
      else {
         sq64 [l][c] = charToInt (cChar);
         if (cChar == 'K' && !bCastleW) sq64 [l][c] = -CASTLEKING; // le roi blanc ne peut plus rioquer
         if (cChar == 'k' && !bCastleB) sq64 [l][c] = CASTLEKING;  // le roi noir a deja roque
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
   /* si le boolean "complete" est vrai alors on transmet le roque, la valeur en passant, */
   /* le compteur des 50 coups et le nb de coups */
   int n, v;
   int i = 0;
   char strCastle [4];
   bool whiteIsCastled = false;
   bool blackIsCastled = false;
   for (int l = N-1; l >=  0; l--) {
      for (int c = 0; c < N; c++) {
         if ((v = sq64 [l][c]) != 0) {
            if (v == CASTLEKING) blackIsCastled = true;
            if (v == -CASTLEKING) whiteIsCastled = true;
            fen [i++] = (v >= 0) ? tolower (dict [v]) : dict [-v];
         }
         else {
            for (n = 0; (c+n < N) && (sq64 [l][c+n] == 0); n++);
            fen [i++] = '0' + n;
            c += n - 1;
         }
      }
      fen [i++] = '/';
   }
   i -= 1;
   fen [i] = '\0';
   sprintf (fen, "%s%c%c", fen, sep, (color == 1) ? 'b': 'w');
   if (complete) {
      castleToStr (whiteIsCastled, blackIsCastled, strCastle);
      sprintf (fen, "%s%c%s%c%s%c%d%c%d", fen, sep, strCastle, sep, ep, sep, cpt50, sep, nb); 
   }
   return fen;
}

void moveGame (TGAME sq64, int color, char *move) { /* */
   /* modifie jeu avec le deplacement move */
   /* move en notation algébrique Pa2-a4 ou Pa2xc3 */
   /* tolere e2e4 e2-e4 e:e4 e2xe4 */
   int base = (color == WHITE) ? 0 : 7;       // Roque non teste
   int cDep, lDep, cDest, lDest, i, j;
   
   if (strncmp (move, "O-O-O", 5) == 0) {     // grand Roque
      sq64 [base][4] = 0;
      sq64 [base][3] = ROOK * color;
      sq64 [base][2] = KING * color;
      sq64 [base][0] = 0;
      return;
   }
   if (strncmp (move, "O-O", 3) == 0) {       // petit Roque
      sq64 [base][4] = 0;
      sq64 [base][5] = ROOK * color;
      sq64 [base][6] = KING * color;
      sq64 [base][7] = 0;
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
   sq64 [lDep][cDep] = 0;
}

bool opening (const char *fileName, char *gameFen, char *sComment, char *move) { /* */
   /* lit le fichier des ouvertures et produit le jeu final */
   /* ce fichier est au format CSV : FENstring ; dep ; commentaire */
   /* dep contient le deplacement en notation algebrique complete Xe2:e4[Y] | O-O | O-O-O */
   /* X : piece joue. Y : promotion,  O-O : petit roque,  O-O-O : grand roque */
   /* renseigne la chaine move (deplacement choisi) et le commentaire associe */
   /* renvoie vrai si ouverture trouvee dans le fichier, faux sinon */
   FILE *fe;
   char line [MAXBUFFER];
   char *ptComment = sComment;
   char *ptDep = move;
   char *sFEN;
   unsigned int lenGameFen = strlen (gameFen);
   if ((fe = fopen (fileName, "r")) == NULL) return false;
   while (fgets (line, MAXBUFFER, fe) != NULL) {
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

bool openingAll (const char *dir, const char *filter, char *gameFen, char *sComment, char *move) {
   /* liste les fichier du repertoire dir contenant la chaine filter */
   /* fichier dans l'ordre alphabetique */
   /* donc nommer les fichiers les plus prioritaires en debut d'alphabet */
   /* appelle opening sur les fichiers listes jusqu'a trouver */
   /* renvoie vrai si gameFen est trouvee dans l'un des fichiers faux sinon */
   struct dirent **namelist;
   int n;
   char fileName [MAXBUFFER];
   char comment [MAXBUFFER];
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

bool symetryV (TGAME sq64, int l1, int c1, int cDest) { /* */ 
   /* vraie si il y a une piece egale a celle pointee par l1, c1 dans le symetrique par rapport a la colonne cDest */
   int cSym = cDest + cDest - c1;
   return (cSym >= 0 && cSym < N) ? (sq64 [l1][c1] == sq64 [l1][cSym]) : false;
}

bool symetryH (TGAME sq64, int l1, int c1, int lDest) { /* */ 
   /* vraie si il y a une piece egale a celle pointee par l1, c1 dans le symetrique par rapport a la ligne lDest */
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
   strcpy (abbr, "");
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
            if (sq64 [l2][i] != 0) break;
         }
      }
      if ((l1 == l2) && (c1 > c2)) {            // meme ligne, recherche a droite  
         for (int i = (c2 - 1); i >= 0; i--) {
            if (sq64 [l1][i] == v) {            // il y a une autre tour capable d'aller vers l2 c2
               sprintf (spec, "%c", c1 + 'a');  // Trouve. On donne la colonne
               break;
            }
            if (sq64 [l2][i] != 0) break;
         }
      }
      if ((c1 == c2) && (l1 < l2)) {            // meme colonne, recherche en bas 
         for (int i = (l2 + 1); i < N; i++) {
            if (sq64 [i][c1] == v) {            // il y a une autre tour capable d'aller vers l2 c2
               sprintf (spec, "%c", l1 + '1');
               break;
            }
            if (sq64 [i][c2] != 0) break;
         }
      }
      if ((c1 == c2) && (l1 > l2)) {            // meme colonne, recherce en haut  
         for (int i = (l2 - 1); i >= 0; i--) {
            if (sq64 [i][c1] == v) {            // il y a une autre tour capable  d'aller vers l2 c2
               sprintf (spec, "%c", l1 + '1');
               break;
            }
            if (sq64 [i][c2] != 0) break;
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

void sendGame (bool http, const char *fen, int reqType) { /* */
   /* envoie le jeu decrit par fen et les struct computer et info au format JSON */
   int k;
   if (http) {
      printf ("Access-Control-Allow-Origin: *\n"); // obligatoire !
      printf ("Cache-Control: no-cache\n");        // eviter les caches
      printf ("Content-Type: text/html; charset=utf-8\n\n");
   }
   printf ("{\n");
   printf ("\"description\" : \"%s\",\n", DESCRIPTION);
   printf ("\"compilation-date\" : \"%s\",\n", __DATE__);
   printf ("\"version\" : \"%s\"", VERSION);
   if (reqType > 0) {
      printf (",\n");
      printf ("\"clockTime\": %lf,\n", (double) info.nClock/CLOCKS_PER_SEC);
      printf ("\"time\" : %lf,\n", (double) info.computeTime/MILLION);
      printf ("\"nbThread\" : %d,\n", info.nbThread);
      printf ("\"eval\" : %d,\n", info.evaluation);
      printf ("\"computerStatus\" : \"%d : %s\",\n", computer.kingState, strStatus [computer.kingState]);
      printf ("\"playerStatus\" : \"%d : %s\",\n", gamer.kingState, strStatus [gamer.kingState]);
      printf ("\"fen\" : \"%s\",\n", fen);
      if (info.lastCapturedByComputer >= ' ' && info.lastCapturedByComputer <= 'z')
         printf ("\"lastTake\" : \"%c\",\n", info.lastCapturedByComputer);
      else printf ("\"lastTake\" : \"%c\",\n", ' ');
      printf ("\"comment\" : \"%s\",\n", info.comment);
      printf ("\"wdl\" : %d,\n", info.wdl);
      printf ("\"move\" : \"%s\",\n", info.computerPlayC);
      printf ("\"moveA\" : \"%s\",\n", info.computerPlayA);      
      printf ("\"score\" : \"%s\",\n", scoreToStr [info.score]);
      printf ("\"moveList\" : [");
      for (k = 0; k < computer.nValidPos - 1; k++) {
         if ((k % 7) == 0) printf ("\n   ");
         printf ("\"%s\", %5d, ", info.moveList [k].strMove, info.moveList [k].eval);
      }
      if ((k % 7) == 0) printf ("\n   ");
      printf ("\"%s\", %5d\n]", info.moveList [computer.nValidPos -1 ].strMove, info.moveList [computer.nValidPos - 1].eval);
   }
   if (reqType > 1) {
      printf (",\n\"dump\" : {\n   ");
      printf ("\"maxDepth\" : %d, ", info.maxDepth);
      printf ("\"nEvalCall\" : %d, ", info.nEvalCall);
      printf ("\"nAlphaBeta\" : %d, ", info.nAlphaBeta);
      printf ("\"nBestNote\" : %d, ", info.nBestNote);
      printf ("\"nValidGamerPos\" : %d, ", gamer.nValidPos);
      printf ("\"nValidComputerPos\" : %d,\n   ", computer.nValidPos);
      printf ("\"nbTrTa\" : %d, ", info.nbTrTa);
      printf ("\"nbMatchTrans\" : %d, ", info.nbMatchTrans);
      printf ("\"nCollision\" : %d, ", info.nbColl);
      printf ("\"nCallfHash\" : %d, ", info.nbCallfHash);
      printf ("\"nLCKingInCheck\" : %d, ", info.nLCKingInCheckCall);
      printf ("\"nBuildList\" : %d\n", info.nBuildListCall);
      printf ("}");
   }
   printf ("\n}\n");
}
