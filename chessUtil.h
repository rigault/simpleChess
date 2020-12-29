#define MILLION 1000000
#define VERSION "2.1"
#define DESCRIPTION "Chess Rene Rigault 2020"
#define PATHTABLE "/var/www/html/chessdata"           // table de fin de jeux SYZYGY.
#define OPENINGDIR "/home/rr/git/simplechess/bigfen"  // repetoire des ouvertures

#define N 8
#define MAXLEN 10000
#define MAXSIZELIST 128
#define GAMESIZE 64         // taille du jeu = N * N * sizeeof (char) = 8 * 8 * 1 ATTENTION PORTABILITE
#define F_LOG "chess.log"   // log des jeux
#define MIN(x,y) ((x<y)?(x):(y))

typedef char TGAME [N][N];  // jeu de 8 x 8 cases codant une piece sur un entier 8 bits avec signe
typedef char TLIST [MAXSIZELIST][N][N];

enum {VOID, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, CASTLEKING};
enum kingState {NOEXIST, EXIST, ISINCHECK, UNVALIDINCHECK, ISMATE, ISPAT};

TLIST list;
int nextL;

FILE *flog;

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
   int note;                     // evaluation du jeu ourant
   int evaluation;               // evaluation rendue par le minimax
   char computerPlayC [15];       // dernier jeu ordi reconstruit par la fonction difference. Notation Alg. complete
   char computerPlayA [15];       // dernier jeu ordi reconstruit par la fonction difference. Notation Alg. abegee
   char lastCapturedByComputer;  // derniere piece prise par Ordi
   int calculatedMaxDepth;
   int lGamerKing;
   int cGamerKing;
   int lComputerKing;
   int cComputerKing;
   int gamerColor;               // -1 si joueur blanc (defaut), 1 si joueur noir
   enum kingState gamerKingState;
   enum kingState computerKingState;
   bool castleGamer;
   bool castleComputer;
   char comment [MAXLEN];        // nom de l'ouverture si trouvee ou fin de partie
   char endName [MAXLEN];        // nom de la database de fermeture si trouvee
   long computeTime;
   clock_t nClock;
   char move [15];               // deplacement donne par fonction ouverture
   unsigned wdl;                 // retour de syzygy - end table
   bool end;
   char epComputer [3];
   char epGamer [3];
} info;

extern int charToInt (int c);
extern int fenToGame (char *fenComplete, TGAME sq64, char *ep, int *cpt50, int *nb);
extern char *gameToFen (TGAME sq64, char *fen, int color, char sep, bool complete, char *ep, int cpt50, int nb);
extern bool openingAll (const char *dir, const char *filter, char *gameFen, char *sComment, char *move);
extern bool opening (const char *fileName, char *gameFen, char *sComment, char *move);
extern char *difference (TGAME jeu1, TGAME jeu2, int color, char *prise, char *complete, char *abbr, char *epGamer, char *epComputer);
extern void sendGame (const char *fen, struct sinfo info, int reqType);
extern void printGame (TGAME jeu, int eval);
extern void moveGame (TGAME jeu, int color, char *move);
