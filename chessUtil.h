#define VERSION "2.1"
#define DESCRIPTION "Chess Rene Rigault 2020"
#define PATHTABLE "/var/www/html/chessdata"    // table de fin de jeux SYZYGY.
#define F_OUVB "../chessopenings/chessB.fen"   // fichier des ouvertures (openings)
#define F_OUVW "../chessopenings/chessW.fen"   // fichier des ouvertures (openings)
#define N 8
#define MAXLEN 1000
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
   int nGamerPieces;             // nombre de pieces Joueur
   int nComputerPieces;                // nombre de pieces Ordi
   int maxDepth;                 // profondeur (minimax)
   int nEvalCall;
   int nLCKingInCheckCall;
   int nBuildListCall;
   int nValidGamerPos;
   int nValidComputerPos;
   int note;                     // evaluation du jeu ourant
   int evaluation;               // evaluation rendue par le minimax
   char computerPlay [15];       // dernier jeu ordi reconstruit par la fonction difference
   int lastCapturedByComputer;   // derniere piece prise par Ordi
   int calculatedMaxDepth;
   int lGamerKing;
   int cGamerKing;
   int lComputerKing;
   int cComputerKing;
   int gamerColor;               // -1 si joueur blanc, 1 si joueur noir
   enum kingState gamerKingState;
   enum kingState computerKingState;
   bool castleGamer;
   bool castleComputer;
   char comment [MAXLEN];        // nom de l'ouverture si trouvee ou fin de partie
   char endName [MAXLEN];        // nom de la database de fermeture si trouvee
   int computeTime;
   clock_t nClock;
   char move [15];               // deplacement donne par fonction ouverture
} info;

extern void fenToGame (char *sFenComplete, TGAME jeu, char *activeColor);
extern void gameToFen (TGAME jeu, char *sFen, int color, char sep, bool complete);
extern bool opening (const char *fileName, char *gameFen, char *sComment, char *move);
extern char *difference (TGAME jeu1, TGAME jeu2, int color, int *prise, char* temp);
extern void sendGame (TGAME jeu, struct sinfo info, int reqType);
extern void moveGame (TGAME jeu, int color, char *move);
