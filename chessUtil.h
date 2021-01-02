#define NIL -9999999
#define MILLION 1000000
#define VERSION "2.1"
#define DESCRIPTION "Chess Rene Rigault 2021"
#define PATHTABLE "/var/www/html/chessdata"           // table de fin de jeux SYZYGY.
#define OPENINGDIR "/home/rr/git/simplechess/bigfen"  // repertoire des ouvertures
#define N 8
#define MAXSIZELIST 128            // taille max liste des jeux
#define MAXTHREADS 128             // nombre max de thread. Garder ces deux valeurs egales.
#define GAMESIZE 64                // taille du jeu = N * N * sizeeof (char) = 8 * 8 * 1 ATTENTION PORTABILITE
#define F_LOG "chess.log"          // log des jeux
#define HELP "Syntax; sudo ./chess.cgi -i|-r|-h|-p|-t [FEN string] [level]"
#define MAXBUFFER 10000            // tampon de caracteres pour longues chaines
#define MAXLENGTH 255              // pour ligne
#define NDEPTH 3                   // pour fMaxDepth ()
#define MAXPIECESSYZYGY 6
#define MAXNBOPENINGS 8            // on ne regarde pas la biblio ouverture a partir de ce nb de coups
#define KINGINCHECKEVAL 1          // evaluation du gain d'un echec au roi..
#define BONUSCENTER 10             // evaluation du gain d'avoir un cavalier au centre
#define BONUSPAWNAHEAD 4           // evaluation du gain d'avoir un pion avance
#define BONUSBISHOP 10             // evaluation du gain d'avoir deux fous
#define BONUSMOVEROOK 10           // evaluation du gain d'avoir une tour non bloquee
#define MATE 1000000

#define MIN(x,y)      ((x<y)?(x):(y))
#define LINE(z)       ((z) >> 3)   // z / 8 ligne
#define COL(z)        ((z) & 0x07) // z % 8 colonne

typedef char TGAME [N][N];  // jeu de 8 x 8 cases codant une piece sur un entier 8 bits avec signe
typedef char TLIST [MAXSIZELIST][N][N];

enum {VOID, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, CASTLEKING};
enum KingState {NOEXIST, EXIST, ISINCHECK, UNVALIDINCHECK, ISMATE, ISPAT};
enum Score {ONGOING, BLACKWIN, DRAW, WHITEWIN};

struct sinfo {
   int nb;                       // nb de coup recus
   int cpt50;                    // compteur pour regle des 50 coups 
   int nGamerPieces;             // nombre de pieces Joueur
   int nComputerPieces;          // nombre de pieces Ordi
   int maxDepth;                 // profondeur (minimax)
   int nEvalCall;
   int nLCKingInCheckCall;
   int nBuildListCall;
   int nValidGamerPos;
   int nValidComputerPos;
   int nMaxList;                 // taille max liste
   int note;                     // evaluation du jeu ourant
   int evaluation;               // evaluation rendue par le minimax
   char computerPlayC [15];      // dernier jeu ordi reconstruit par la fonction difference. Notation Alg. complete
   char computerPlayA [15];      // dernier jeu ordi reconstruit par la fonction difference. Notation Alg. abegee
   char lastCapturedByComputer;  // derniere piece prise par Ordi
   int calculatedMaxDepth;
   int lGamerKing;
   int cGamerKing;
   int lComputerKing;
   int cComputerKing;
   int gamerColor;               // -1 si joueur blanc (defaut), 1 si joueur noir
   enum KingState gamerKingState;
   enum KingState computerKingState;
   bool castleGamer;
   bool castleComputer;
   char comment [MAXBUFFER];     // nom de l'ouverture si trouvee ou fin de partie
   char endName [MAXBUFFER];     // nom de la database de fermeture si trouvee
   long computeTime;
   clock_t nClock;
   char move [15];               // deplacement donne par fonction ouverture
   unsigned wdl;                 // retour de syzygy - end table
   char epComputer [3];
   char epGamer [3];
   enum Score score;
   int  nBestNote;               // nombre de podsibilites ayant la meilleure eval
} info;

extern int fenToGame (char *fenComplete, TGAME sq64, char *ep, int *cpt50, int *nb);
extern char *gameToFen (TGAME sq64, char *fen, int color, char sep, bool complete, char *ep, int cpt50, int nb);
extern bool openingAll (const char *dir, const char *filter, char *gameFen, char *sComment, char *move);
extern char *difference (TGAME jeu1, TGAME jeu2, int color, char *prise, char *complete, char *abbr, char *epGamer, char *epComputer);
extern void sendGame (const char *fen, struct sinfo info, int reqType);
extern void moveGame (TGAME jeu, int color, char *move);
extern void printGame (TGAME jeu, int eval);
