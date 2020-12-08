# simpleChess
Ce jeu d'échec s'exécute sur un serveur. Il est écrit en langage C. Une API est publiée permettant à quiconque de développer la partie cliente indépendemment du serveur.

Une API restful est définie et exposée par le serveur.
La requête est de type GET avec les paramètres apparents dans l'URL.
La réponse est au format Jason.
Le serveur ne gère pas d'état. Il répond aux requêtes sans utiliser d'informations en mémoires.
La requête un type, la totalité du jeu d'échecs au format ASCII FEN, et un niveau de profondeur de jeu.



