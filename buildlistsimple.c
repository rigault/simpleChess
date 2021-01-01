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
   register int u, v, i, j, k, l, c;
   register int nListe = 0;
   char *pl = &list [0][0][0];
   info.nBuildListCall += 1;
   int base = (who == -1) ? 0 : 7;

   // Roque
   if (refJeu [base][4] == KING) {
      // roque gauche
      if (refJeu [base][0] == ROOK && 0 == refJeu [base][1] && 0 == refJeu [base][2] && 0 == refJeu [base][3] &&
         !LCkingInCheck (refJeu, who, base, 2) &&  !LCkingInCheck (refJeu, who, base, 3) && !LCkingInCheck (refJeu, who, base, 4)) {
         // les cases traversees par le roi ne sont pas echec au roi
         memcpy (pl, refJeu, GAMESIZE);
         list [nListe][base][0] = 0;
         list [nListe][base][2] = CASTLEKING;
         list [nListe][base][3] = ROOK;
         list [nListe++][base][4] = 0; 
         pl += GAMESIZE;
      }
      // Roque droit
      if (refJeu [base][7] == ROOK && 0 == refJeu [base][5] && 0  == refJeu [base][6] &&
         !LCkingInCheck (refJeu, who, base, 4) && !LCkingInCheck (refJeu, who, base, 5) && !LCkingInCheck (refJeu, who, base, 6)) {
         // les cases traversees par le roi ne sont pas echec au roi
         memcpy (pl, refJeu, GAMESIZE);
         list [nListe][base][4] = 0;
         list [nListe][base][5] = ROOK;
         list [nListe][base][6] = CASTLEKING;
         list [nListe++][base][7] = 0;
         pl += GAMESIZE;
      }
   }
   for (l = 0; l < N; l++) {
      for (c = 0; c < N; c++) {
         u = refJeu [l][c];
         v = who * u; // v = abs (u)
         if (v > 0)
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
            for (i = l+1; i < N; i++) { // en haut
               if (u * refJeu [i][c] <= 0) {
                  pl = pushList (refJeu, list, nListe++, pl, l, c, i, c, u);
                  if (refJeu [i][c] != 0)  break;
               }
               else break;
            }
            for (i = l-1; i >=0; i--) { // en bas
               if (u * refJeu [i][c] <= 0) {
                  pl = pushList (refJeu, list, nListe++, pl, l, c, i, c, u);
                  if (refJeu [i][c] != 0) break;
               }
               else break;
            }
            for (j = c+1; j < N; j++) {  // a droite
               if (u * refJeu [l][j] <= 0) {
                  pl = pushList (refJeu, list, nListe++, pl, l, c, l, j, u);
                  if (refJeu [l][j] != 0) break;
               }
               else break;
            }
            for (j = c-1; j >=0; j--)  { // a gauche
               if (u * refJeu [l][j] <= 0) {
                  pl = pushList (refJeu, list, nListe++, pl, l, c, l, j, u);
                  if (refJeu [l][j] != 0) break;
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
      }    // fin des deux for imbriques
   }
   if ((nextL + nListe) > info.nMaxList) info.nMaxList = nextL + nListe;
   return nListe;
}

