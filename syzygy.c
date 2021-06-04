/*! \mainpage inspire de Fathom, adapte par RENE RIGAULT */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tbprobe.h"
#include "syzygy.h"

#define BOARD_RANK_1  0x00000000000000FFull
#define BOARD_FILE_A  0x8080808080808080ull
#define SQUARE(r, f)  (8 * (r) + (f))
#define RANK(s)       ((s) >> 3)
#define FILE_(s)      ((s) & 0x07)
#define BOARD(s)      ((uint64_t)1 << (s))

static const char *WDL_TO_STR[] = { "0-1", "1/2-1/2", "1/2-1/2", "1/2-1/2", "1-0" };

struct Pos {
   uint64_t white;
   uint64_t black;
   uint64_t kings;
   uint64_t queens;
   uint64_t rooks;
   uint64_t bishops;
   uint64_t knights;
   uint64_t pawns;
   uint8_t castling;
   uint8_t rule50;
   uint8_t ep;
   bool turn;
   uint16_t move;
};

/*! traduit la chaine au format FEN en une structure pos. ATTENTION DOUTE SUR CASTLING */
static bool parseFEN (struct Pos *pos, const char *fen) { /* */
   uint64_t white = 0, black = 0;
   uint64_t kings, queens, rooks, bishops, knights, pawns;
   bool turn;
   unsigned rule50 = 0, move = 1;
   unsigned ep = 0;
   unsigned castling = 0;
   unsigned file, rank;
   char c;
   int r, f;
   char clk[4];
   kings = queens = rooks = bishops = knights = pawns = 0;

   if (fen == NULL) return false;

   for (r = 7; r >= 0; r--) {
      for (f = 0; f <= 7; f++) {
         unsigned s = (r * 8) + f;
         uint64_t b = BOARD(s);
         c = *fen++;
         switch (c) {
            case 'k': kings |= b; black |= b; continue;
            case 'K': kings |= b; white |= b; continue;
            case 'q': queens |= b; black |= b; continue;
            case 'Q': queens |= b; white |= b; continue;
            case 'r': rooks |= b; black |= b; continue;
            case 'R': rooks |= b; white |= b; continue;
            case 'b': bishops |= b; black |= b; continue;
            case 'B': bishops |= b; white |= b; continue;
            case 'n': knights |= b; black |= b; continue;
            case 'N': knights |= b; white |= b; continue;
            case 'p': pawns |= b; black |= b; continue;
            case 'P': pawns |= b; white |= b; continue;
            default: break;
         }
         if (c >= '1' && c <= '8') {
            unsigned jmp = (unsigned)c - '0';
            f += jmp-1;
            continue;
         }
         return false;
      }
      if (r == 0) break;
      c = *fen++;
      if (c != '/') return false;
   }
   c = *fen++;
   if (c != ' ') return false;
   c = *fen++;
   if (c != 'w' && c != 'b') return false;
   turn = (c == 'w');
   c = *fen++;

   if (c != ' ') return false;
   c = *fen++;
   if (c != '-') {
      do {
         switch (c) {
            case 'K': castling |= TB_CASTLING_K; break;
            case 'Q': castling |= TB_CASTLING_Q; break;
            case 'k': castling |= TB_CASTLING_k; break;
            case 'q': castling |= TB_CASTLING_q; break;
            default: return false;
         }
         c = *fen++;
      } while (c != ' ');
      fen--;
   }
   c = *fen++;
   if (c != ' ') return false;
   c = *fen++;
   if (c >= 'a' && c <= 'h') {
      file = c - 'a';
      c = *fen++;
      if (c != '3' && c != '6') return false;
      rank = c - '1';
      ep = SQUARE(rank, file);
      if (rank == 2 && turn) return false;
      if (rank == 5 && !turn) return false;
      if (rank == 2 && ((tb_pawn_attacks(ep, true) & (black & pawns)) == 0)) ep = 0;
      if (rank == 5 && ((tb_pawn_attacks(ep, false) & (white & pawns)) == 0)) ep = 0;
   }
   else if (c != '-') return false;
   c = *fen++;
   if (c != ' ') return false;
   clk[0] = *fen++;
   if (clk[0] < '0' || clk[0] > '9') return false;
   clk[1] = *fen++;
   if (clk[1] != ' ') {
      if (clk[1] < '0' || clk[1] > '9') return false;
      clk[2] = *fen++;
      if (clk[2] != ' ') {
         if (clk[2] < '0' || clk[2] > '9') return false;
         c = *fen++;
         if (c != ' ') return false;
         clk[3] = '\0';
      }
      else clk[2] = '\0';
   }
   else clk[1] = '\0';
   rule50 = atoi(clk);
   move = atoi(fen);
   pos->white = white;
   pos->black = black;
   pos->kings = kings;
   pos->queens = queens;
   pos->rooks = rooks;
   pos->bishops = bishops;
   pos->knights = knights;
   pos->pawns = pawns;
   pos->castling = castling;
   pos->rule50 = rule50;
   pos->ep = ep;
   pos->turn = turn;
   pos->move = move;
   return true;
}

/*! Converti un deplacement en une chaine str au format algebrique complet. Ex :  Pe2-e4 */
static void moveToStr (const struct Pos *pos, unsigned move, char *str) { /* */
   uint64_t occ = pos->black | pos->white;
   unsigned from = TB_GET_FROM(move);
   unsigned to = TB_GET_TO(move);
   unsigned r = RANK(from);
   unsigned f = FILE_(from);
   unsigned promotes = TB_GET_PROMOTES(move);
   bool capture  = (occ & BOARD(to)) != 0 || (TB_GET_EP(move) != 0);
   uint64_t b = BOARD(from);
   
   if (b & pos->kings) *str++ = 'K';
   else if (b & pos->queens) *str++ = 'Q';
   else if (b & pos->rooks) *str++ = 'R';
   else if (b & pos->bishops) *str++ = 'B';
   else if (b & pos->knights) *str++ = 'N';
   else *str++ = 'P';
   *str++ = 'a' + f; 
   *str++ = '1' + r; 
   *str++ = (capture) ? 'x' : '-';
   *str++ = 'a' + FILE_(to);
   *str++ = '1' + RANK(to);
   if (promotes != TB_PROMOTES_NONE) {
      *str++ = '='; 
      switch (promotes) {
         case TB_PROMOTES_QUEEN: *str++ = 'Q'; break;
         case TB_PROMOTES_ROOK: *str++ = 'R'; break;
         case TB_PROMOTES_BISHOP: *str++ = 'B'; break;
         case TB_PROMOTES_KNIGHT: *str++ = 'N'; break;
      }
   }
   *str++ = '\0';
}

/*! recherche dans la tablebase sygyzy situee dans path le jeu decrit en notation FEN par fen. 
 * \li Renvoie le deplacement au format algebrique complet dans bestMove
 * \li le commentaire contient ce deplacement et les valeur WIN WDL DTZ
 * \li retourne vrai si trouve, faux si erreur */
bool syzygyRR (const char* path, const char *fen, int *wdl, char *bestMove, char *comment) { /* */
   struct Pos pos0;
   struct Pos *pos = &pos0;
   unsigned move;

   // init
   if (path == NULL) path = getenv ("TB_PATH");
   tb_init (path);
   if (TB_LARGEST == 0) {
      sprintf (comment, "ERR: no tablebase file:");
      return false;
   }

   // parse the FEN
   if (!(parseFEN(pos, fen))) {
      sprintf (comment, "ERR: unable to parse FEN string");
      return false;
   }
   // probe the TB
   if (tb_pop_count(pos->white | pos->black) > TB_LARGEST) {
      sprintf (comment, "ERR: too many pieces max = %u", TB_LARGEST);
      return false;
   }

   move = tb_probe_root(pos->white, pos->black, pos->kings,
     pos->queens, pos->rooks, pos->bishops, pos->knights, pos->pawns,
     pos->rule50, pos->castling, pos->ep, pos->turn, NULL);

   if (move == TB_RESULT_FAILED) {
      sprintf (comment, "ERR: TB probe failed");
      return false;
   }
   if (move == TB_RESULT_CHECKMATE) {
      sprintf (comment, "# %s\n", (pos->turn? "0-1": "1-0"));
      return true;
   }
   if (pos->rule50 >= 100 || move == TB_RESULT_STALEMATE) {
      sprintf (comment, " 1/2-1/2\n");
      return true;
   }
   *wdl = TB_GET_WDL(move);
   // Output
   moveToStr (pos, move, bestMove);
   sprintf (comment, "WIN: %s; BESTMOVE: %s; WDL: %u; DTZ: %u", 
      WDL_TO_STR[(pos->turn? *wdl: 4-*wdl)], bestMove, *wdl, TB_GET_DTZ(move));
   return true;
}
