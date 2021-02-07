#define MILLION 1000000
#define VERSION "2.1"
#define DESCRIPTION "Chess Rene Rigault 2021"
#define PATHTABLE "/var/www/html/chessdata"  // table de fin de jeux SYZYGY.
#define OPENINGDIR "/var/www/html/bigfen"    // repertoire des ouvertures
#define F_LOG "chess.log"                    // log des jeux
#define N 8                                  // nombre de lignes/colonnes
#define MAXSIZELIST 128                      // taille max liste des jeux
#define MAXTHREADS 128                       // nombre max de thread. Garder ces deux valeurs egales.
#define GAMESIZE 64                          // taille du jeu = N * N * sizeeof (int8_t) = 8 * 8 * 1 ATTENTION PORTABILITE
#define HELP "Synopsys: sudo ./chess.cgi -q|-v|-f|-t|-p|-h [FEN string] [level]\n" \
    "More help: firefox|google-chrome|lynx ../front/chessdoc.html"
#define MAXBUFFER 10000                      // tampon de caracteres pour longues chaines
#define MAXLENGTH 255                        // pour ligne
#define MAXSTRMOVE 15                        // Longueur MOVE au format complet
#define MAXPIECESSYZYGY 6                    // a partir de cette valeur on consule les tables endgame syzygy
#define MAXNBOPENINGS 8                      // on ne regarde pas la biblio ouverture a partir de ce nb de coups

#define WHITE (-1)                           // Blancs
#define BLACK 1                              // Noir
#define KINGINCHECKEVAL 1                    // evaluation du gain d'un echec au roi..
#define BONUSCASTLE 10                       // Le roi qui a roque a un bonus
#define BONUSCENTER 1                        // evaluation du gain d'avoir cavalier fou tour reine dans carre central
#define BONUSKNIGHT 1                        // evaluation du gain d'avoir un cavalier eloigne des bords
#define BONUSPAWNAHEAD 2                     // evaluation du gain d'avoir un pion avance
#define BONUSBISHOP 2                        // evaluation du gain d'avoir deux fous
#define BONUSMOVEROOK 2                      // evaluation du gain d'avoir une tour non bloquee
#define MALUSISOLATEDPAWN 2                  // evaluation de la perte d'un pion isole
#define MALUSBLOCKEDPAWN 2                   // evaluation de la perte d'un pion bloque
#define MATE 10000                           // evaluation du MAT

#define MIN(x,y)      ((x<y)?(x):(y))        // minimum
#define LINE(z)       ((z) >> 3)             // z / 8 (pour trouver ligne)
#define COL(z)        ((z) & 0x07)           // z % 8 (pour trouver colonne

typedef int8_t TGAME [N][N];                 // jeu de 8 x 8 cases codant une piece sur un entier 8 bits avec signe

typedef struct {
   int8_t type;
   int8_t who;
   int8_t l1;
   int8_t c1;
   int8_t l2;
   int8_t c2;
   int8_t taken;
} TMOVE;

typedef TMOVE TLISTMOVE [MAXSIZELIST]; 

typedef struct {                             // structure decrivant un deplacement, le jeu resultant et son evaluation.
   TGAME jeu;
   TMOVE move;
   char strMove [MAXSTRMOVE];
   int eval;
   uint64_t zobrist;
} TMOVEINFO;

enum {VOID, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, CASTLEKING};            // VOID car PAWN = 1, ...
enum KingState {NOEXIST, EXIST, ISINCHECK, UNVALIDINCHECK, ISMATE, ISPAT};   // type etat  
enum Score {ERROR, ONGOING, BLACKWIN, DRAW, WHITEWIN};                       // type scores finaux
enum {STD, ENPASSANT, KINGCASTLESIDE, QUEENCASTLESIDE, PROMOTION, CHANGEKING};

struct Sinfo {
   int nbThread;                    // nombre de thread sur l'architecture utilisee
   int nb;                          // nb de coup recus
   int cpt50;                       // compteur pour regle des 50 coups 
   int nPieces;                     // nombre de pieces
   int maxDepth;                    // profondeur (minimax) recalculee
   int nAlphaBeta;                  // nombre d'appels alphaBeta
   int nEvalCall;                   // nombre d'appels Eval
   int nLCKingInCheckCall;          // nombre d'appels nLCLingInCheck (si gere)  
   int nBuildListCall;              // nombre d'appels nBuildList
   int evaluation;                  // evaluation rendue par la fonction d evaluation
   char computerPlayC [MAXSTRMOVE]; // dernier jeu ordi. Notation Alg. complete
   char computerPlayA [MAXSTRMOVE]; // dernier jeu ordi. Notation Alg. abegee
   char lastCapturedByComputer;     // derniere piece prise par Ordi
   int calculatedMaxDepth;          // profondeur max atteinte
   char comment [MAXBUFFER];        // nom de l'ouverture si trouvee ou fin de partie
   long computeTime;                // temps de calcul
   clock_t nClock;                  // utilisation du processeur
   int wdl;                         // retour de syzygy - end table
   enum Score score;                // score : "ERROR' "-" "1-0" "0-1" "1/2-1/2"
   bool pat;                        // retour de evaluation. vrai si le jeu est pat
   int  nBestNote;                  // nombre de possibilites ayant la meilleure eval
   int nbTrTa;                      // nombre de positions occupees dans la table de transposition
   int nbColl;                      // nombre de collisions
   int nbCallfHash;                 // nombre appels fn de hachage.
   int nbMatchTrans;                // nombre de matching transposition  
   int nDoMove;                     // nombre d'appls a doMove  
   TMOVEINFO moveList [MAXSIZELIST];// liste des move possibles et leur evaluation
} info;

struct Player {
   int color;                       // -1 si joueur blanc (defaut), 1 si joueur noir. Note gamer.color uniquement utilise
   int nValidPos;                   // nombre de positions valides trouvee par buildList
   enum KingState kingState;        // etat roi : NOEXIST, ...
   bool kingCastleOK;               // roque cote roi autorise
   bool queenCastleOK;              // roque cote reine autorise
   char ep [3];                     // en passant au format c3
} gamer, computer;

extern void moveToStr (TMOVE move, char str [MAXSTRMOVE]);
extern int fenToGame (char *fenComplete, TGAME sq64, char *ep, int *cpt50, int *nb);
extern char *gameToFen (TGAME sq64, char *fen, int color, char sep, bool complete, char *ep, int cpt50, int nb);
extern bool openingAll (const char *dir, const char *filter, char *gameFen, char *sComment, char *move);
extern char *enPassant (int color, char *complete, char *strEp);
extern char *abbrev (TGAME sq64, char *complete, char *abbr);
extern void sendGame (bool http, const char *fen, int reqType);
extern void moveGame (TGAME jeu, int color, char *move);
extern void printGame (TGAME jeu, int eval);

