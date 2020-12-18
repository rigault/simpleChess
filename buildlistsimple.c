int buildList (TGAME refJeu, register int who, TLIST list) { /* */
   /* construit la liste des jeux possibles a partir de jeu. */
   /* 'who' joue */
   register int u, v, i, j, k, l, c;
   register int nListe = 0;
   char *pl = &list [0][0][0];
   info.nBuildListCall += 1;

   if (who == 1) {
      if (refJeu [7][4] == KING) {
         if (refJeu [7][0] == ROOK && 0 == refJeu [7][1] && 0 == refJeu [7][2] && 0 == refJeu [7][3] &&
            !LCkingInCheck (refJeu, who, 7, 2) &&  ! LCkingInCheck (refJeu, who, 7, 3) && !LCkingInCheck (refJeu, who, 7, 4)) {
            // les cases traversees par le roi ne sont pas echec au roi
            memcpy (pl, refJeu, GAMESIZE);
            list [nListe][7][0] = 0;
            list [nListe][7][2] = CASTLEKING;
            list [nListe][7][3] = ROOK;
            list [nListe++][7][4] = 0; 
            pl += GAMESIZE;
         }
         // Roque ordi droit
         if (refJeu [7][7] == ROOK && 0 == refJeu [7][5] && 0  == refJeu [7][6] &&
            !LCkingInCheck (refJeu, who, 7, 4) && LCkingInCheck (refJeu, who, 7, 5) && !LCkingInCheck (refJeu, who, 7, 6)) {
            // les cases traversees par le roi ne sont pas echec au roi
            memcpy (pl, refJeu, GAMESIZE);
            list [nListe][7][4] = 0;
            list [nListe][7][5] = ROOK;
            list [nListe][7][6] = CASTLEKING;
            list [nListe++][7][7] = 0;
            pl += GAMESIZE;
         }
      }
   }
   // Roque opposant gauche
   else { // who == -1
      if (refJeu [0][4] == -KING) {
         if (refJeu [0][0] == -ROOK && 0 == refJeu [0][1] && 0 == refJeu [0][2] && 0 == refJeu [0][3] &&
            !LCkingInCheck (refJeu, who, 0, 2) &&  ! LCkingInCheck (refJeu, who, 0, 3) &&! LCkingInCheck (refJeu, who, 0, 4)) {
            // les cases traversees par le roi ne sont pas echec au roi
            memcpy (pl, refJeu, GAMESIZE);
            list [nListe][0][0] = 0;
            list [nListe][0][2] = -CASTLEKING;
            list [nListe][0][3] = -ROOK;
            list [nListe++][0][4] = 0;
            pl += GAMESIZE;
         }
         // Roque opposant droit
         if (refJeu [0][7] == -ROOK && 0 == refJeu [0][5] && 0 == refJeu [0][6] &&
            !LCkingInCheck (refJeu, who, 0, 4) && ! LCkingInCheck (refJeu, who, 0, 5) && !LCkingInCheck (refJeu, who, 0, 6)) {
            // les cases traversees par le roi ne sont pas echec au roi
            memcpy (pl, refJeu, GAMESIZE);
            list [nListe][0][4] = 0;
            list [nListe][0][5] = -ROOK;
            list [nListe][0][6] = -CASTLEKING;
            list [nListe++][0][7] = 0;
            pl += GAMESIZE;
         }
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
            if (who == -1 && l == 1 && 0 == refJeu [l+1][c] && 0 == refJeu[l+2][c]) { // coup initial saut de 2 cases 
               memcpy (pl, refJeu, GAMESIZE);
               list [nListe][l+2][c] = -PAWN;
               list [nListe][l][c] = 0;
               pl += GAMESIZE; nListe += 1;
            }
            if (who == 1 && l == 6 && 0 == refJeu [l-1][c] && 0 == refJeu [l-2][c]) { // coup initial saut de 2 cases
               memcpy (pl, refJeu, GAMESIZE);
               list [nListe][l-2][c] = PAWN;
               list [nListe][l][c] = 0;
               pl += GAMESIZE; nListe += 1;
            }
            if (((l-who) >= 0) && ((l-who) < 8) && (0 == refJeu [l-who][c])) { // normal
               memcpy (pl, refJeu, GAMESIZE);
               if (l-1 == 0 && who == 1)  list [nListe][l-1][c] = QUEEN;
               else if (l+1 == 7 && who == -1) list [nListe][l+1][c] = -QUEEN;
               else list [nListe][l-who][c] = PAWN * who;
               list [nListe][l][c] = 0;
               pl += GAMESIZE; nListe += 1;
            }
            // prise a droite
            if (c < 7 && (l-who) >=0 && (l-who) < N && refJeu [l-who][c+1]*who < 0) { // signes opposes
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
               memcpy (pl, refJeu, GAMESIZE);
               list [nListe][l][c+1] = who * CASTLEKING; // plus droit de roquer
               list [nListe][l][c] = 0;
               pl += GAMESIZE; nListe += 1;
            }
            if (c > 0 && (u * refJeu [l][c-1] <= 0)) {
               memcpy (pl, refJeu, GAMESIZE);
               list [nListe][l][c-1] = who * CASTLEKING;
               list [nListe][l][c] = 0;
               pl += GAMESIZE; nListe += 1;
            }
            if (l < 7 && (u * refJeu [l+1][c] <= 0)) {
               memcpy (pl, refJeu, GAMESIZE);
               list [nListe][l+1][c] = who * CASTLEKING;
               list [nListe][l][c] = 0;
               pl += GAMESIZE; nListe += 1;
            }
            if (l > 0 && (u * refJeu [l-1][c] <= 0)) {
               memcpy (pl, refJeu, GAMESIZE);
               list [nListe][l-1][c] = who * CASTLEKING;
               list [nListe][l][c] = 0;
               pl += GAMESIZE; nListe += 1;
            }
            if ((l < 7) && (c < 7) && (u * refJeu [l+1][c+1] <= 0)) {
               memcpy (pl, refJeu, GAMESIZE);
               list [nListe][l+1][c+1] = who * CASTLEKING;
               list [nListe][l][c] = 0;
               pl += GAMESIZE; nListe += 1;
            }
            if ((l < 7) && (c > 0) && (u * refJeu [l+1][c-1] <= 0)) {
               memcpy (pl, refJeu, GAMESIZE);
               list [nListe][l+1][c-1] = who * CASTLEKING;
               list [nListe][l][c] = 0;
               pl += GAMESIZE; nListe += 1;
            }
            if ((l > 0) && (c < 7) && (u * refJeu [l-1][c+1] <= 0)) {
               memcpy (pl, refJeu, GAMESIZE);
               list [nListe][l-1][c+1] = who * CASTLEKING;
               list [nListe][l][c] = 0;
               pl += GAMESIZE; nListe += 1;
            }
            if ((l > 0) && (c > 0) && (u * refJeu [l-1][c-1] <= 0)) {
               memcpy (pl, refJeu, GAMESIZE);
               list [nListe][l-1][c-1] = who * CASTLEKING;
               list [nListe][l][c] = 0;
               pl += GAMESIZE; nListe += 1;
            }
            break;

         case KNIGHT:
            if (l < 7 && c < 6 && (u * refJeu [l+1][c+2] <= 0)) {
               memcpy (pl, refJeu, GAMESIZE);
               list [nListe][l+1][c+2] = u;
               list [nListe][l][c] = 0;
               pl += GAMESIZE; nListe += 1;
            }
            if (l < 7 && c > 1 && (u * refJeu [l+1][c-2] <= 0)) {
               memcpy (pl, refJeu, GAMESIZE);
               list [nListe][l+1][c-2] = u;
               list [nListe][l][c] = 0;
               pl += GAMESIZE; nListe += 1;
            }
            if (l < 6 && c < 7 && (u * refJeu [l+2][c+1] <= 0)) {
               memcpy (pl, refJeu, GAMESIZE);
               list [nListe][l+2][c+1] = u;
               list [nListe][l][c] = 0;
               pl += GAMESIZE; nListe += 1;
            }
            if (l < 6 && c > 0 && (u * refJeu [l+2][c-1] <= 0)) {
               memcpy (pl, refJeu, GAMESIZE);
               list [nListe][l+2][c-1] = u;
               list [nListe][l][c] = 0;
               pl += GAMESIZE; nListe += 1;
            }
            if (l > 0 && c < 6 && (u * refJeu [l-1][c+2] <= 0)) {
               memcpy (pl, refJeu, GAMESIZE);
               list [nListe][l-1][c+2] = u;
               list [nListe][l][c] = 0;
               pl += GAMESIZE; nListe += 1;
            }
            if (l > 0  && c > 1 && (u * refJeu [l-1][c-2] <= 0)) {
               memcpy (pl, refJeu, GAMESIZE);
               list [nListe][l-1][c-2] = u;
               list [nListe][l][c] = 0;
               pl += GAMESIZE; nListe += 1;
            }
            if (l > 1 && c < 7 && (u * refJeu [l-2][c+1] <= 0)) {
               memcpy (pl, refJeu, GAMESIZE);
               list [nListe][l-2][c+1] = u;
               list [nListe][l][c] = 0;
               pl += GAMESIZE; nListe += 1;
            }
            if (l > 1 && c > 0 && (u * refJeu [l-2][c-1] <= 0)) {
               memcpy (pl, refJeu, GAMESIZE);
               list [nListe][l-2][c-1] = u;
               list [nListe][l][c] = 0;
               pl += GAMESIZE; nListe += 1;
            }
            break;

         case ROOK: case QUEEN:
            for (i = l+1; i < N; i++) { // en haut
               if (u * refJeu [i][c] <= 0) {
                  memcpy (pl, refJeu, GAMESIZE);
                  list [nListe][i][c] = u;
                  list [nListe][l][c] = 0;
                  pl += GAMESIZE; nListe += 1;
                  if (refJeu [i][c] != 0)  break;
               }
               else break;
            }
            for (i = l-1; i >=0; i--) { // en bas
               if (u * refJeu [i][c] <= 0) {
                  memcpy (pl, refJeu, GAMESIZE);
                  list [nListe][i][c] = u;
                  list [nListe][l][c] = 0;
                  pl += GAMESIZE; nListe += 1;
                  if (refJeu [i][c] != 0) break;
               }
               else break;
            }
            for (j = c+1; j < N; j++) {  // a droite
               if (u * refJeu [l][j] <= 0) {
                  memcpy (pl, refJeu, GAMESIZE);
                  list [nListe][l][j] = u;
                  list [nListe][l][c] = 0;
                  pl += GAMESIZE; nListe += 1;
                  if (refJeu [l][j] != 0) break;
               }
               else break;
            }
            for (j = c-1; j >=0; j--)  { // a gauche
               if (u * refJeu [l][j] <= 0) {
                  memcpy (pl, refJeu, GAMESIZE);
                  list [nListe][l][j] = u;
                  list [nListe][l][c] = 0;
                  pl += GAMESIZE; nListe += 1;
                  if (refJeu [l][j] != 0) break;
               }
               else break;
            }
            if (v == ROOK) break;
         // surtout pas de break pour la reine
         case BISHOP : // valable aussi pour QUEEN
            for (k = 0; k < MIN (7-l, 7-c); k++) { // vers haut droit
               if (u * refJeu [l+k+1][c+k+1] <=0) {
                  memcpy (pl, refJeu, GAMESIZE);
                  list [nListe][l+k+1][c+k+1] = u;
                  list [nListe][l][c] = 0;
                  pl += GAMESIZE; nListe += 1;
                  if (refJeu [l+k+1][c+k+1] != 0) break;
               }
               else break;
            }
            for (k = 0; k < MIN (7-l, c); k++) { // vers haut gauche
               if (u * refJeu [l+k+1][c-k-1] <=0) {
                  memcpy (pl, refJeu, GAMESIZE);
                  list [nListe][l+k+1][c-k-1] = u;
                  list [nListe][l][c] = 0;
                  pl += GAMESIZE; nListe += 1;
                  if (refJeu [l+k+1][c-k-1] != 0) break;
               }
               else break;
            }
            for (k = 0; k < MIN (l, 7-c); k++) { // vers bas droit
               if (u * refJeu [l-k-1][c+k+1] <=0) {
                  memcpy (pl, refJeu, GAMESIZE);
                  list [nListe][l-k-1][c+k+1] = u;
                  list [nListe][l][c] = 0;
                  pl += GAMESIZE; nListe += 1;
                  if (refJeu [l-k-1][c+k+1] != 0) break;
               }
               else break;
            }
            for (k = 0; k < MIN (l, c); k++) { // vers bas gauche
               if (u * refJeu [l-k-1][c-k-1] <=0) {
                  memcpy (pl, refJeu, GAMESIZE);
                  list [nListe][l-k-1][c-k-1] = u;
                  list [nListe][l][c] = 0;
                  pl += GAMESIZE; nListe += 1;
                  if (refJeu [l-k-1][c-k-1] != 0) break;
               }
               else break;
            }
            break;
         default:;
         } //fin du switch
      }    // fin des deux for imbriques
   }
   return nListe;
}

