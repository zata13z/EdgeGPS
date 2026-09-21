# Audit de préparation — 21 septembre 2026

## Périmètre

Copie des sept fichiers source du sketch actuel, et non de l'archive initiale.
Ni la carte ni le sketch de travail d'origine ne sont modifiés par cette préparation.
Le document de préparation fourni sert de référence. La publication, initialement privée,
a ensuite été rendue publique sur autorisation explicite. Aucun historique Git n'existait dans le dossier source.

## Résultats et changements

- Mot de passe du point d'accès EdgeGPS volontairement livré dans le code et le README,
  sur demande explicite du propriétaire. Ce n'est pas un identifiant de box personnelle.
  `config.h`, facultatif et exclu de Git, permet une surcharge locale. Contrôle de longueur à la compilation.
- Aucun fichier de parcours, dump de mémoire, log, sauvegarde ou binaire embarqué.
- Pas d'identifiant personnel, chemin de compte local, e-mail personnel, clé privée ou
  jeton identifié dans les sources livrées par inspection et recherche textuelle.
- Coordonnées géographiques intégrées : bases de communes publiques et simulation
  explicitement fictive, pas un historique de déplacement personnel.
- Documentation, licence Apache officielle intégrale, NOTICE, réserves tiers,
  exclusions Git et test d'interface ajoutés. Aucun fichier original supprimé.
- Dépôt Git local initialisé sur main ; publication publique : ZataLabs/EdgeGPS.
- Identité de publication : ZataLabs et adresse GitHub noreply, sans e-mail personnel.

## Contrôles et portée

Version corrigée : compilation sans config.h réussie (1 977 629 octets programme,
64 672 octets de variables globales). Tests JavaScript réussis, y compris cohérence
du mot de passe AP entre code et README et présence de toutes les routes documentées.
Le contrôle de confidentialité distingue désormais le mot de passe AP explicitement
autorisé des informations à exclure (chemins personnels, identifiants de box, clés privées, jetons).
`git add --dry-run .` confirme la liste des fichiers ; `git check-ignore` confirme
l'exclusion de config.h, .env, GPX et binaires. Seuls les fichiers examinés sont destinés au commit.

Le test `node tests/interface.cjs` vérifie la syntaxe JavaScript, les identifiants DOM,
les états démarrage/pause/stockage, l'historique, une action asynchrone et la conversion
de moyenne. Il utilise un DOM simulé : ce n'est pas un test navigateur ou réseau réel.
La nouvelle compilation utilise les versions locales documentées et la configuration
AP par défaut, sans config.h et sans téléversement sur la carte.
L'installation complète des dépendances dans une machine vierge n'a pas été testée.

## Réserves avant publication / déploiement

- Vérifier provenance, millésime et licence exacte des données géographiques embarquées.
- Vérifier les obligations LGPL et les autres composants avant diffusion de binaires.
- Aucun audit exhaustif CVE, fuzzing, analyse statique C++ complète ou test de pénétration
  réalisé ; ne pas présenter le projet comme certifié sans vulnérabilités.
- HTTP non chiffré et commandes sans authentification applicative ; pas d'exposition Internet.
- Parcours et identifiants mémorisés restent présents sur la carte, hors périmètre du paquet.
- Le mot de passe AP par défaut est connu des lecteurs du dépôt. Le personnaliser
  si la confidentialité du réseau est requise ; la carte n'est pas reflashée ici.
- Pannes d'alimentation, journal partiellement écrit et saturation : essais matériels à
  poursuivre. Le journal peut se mettre en erreur plutôt que réparer un fichier endommagé.
- Une recherche textuelle ne garantit pas mathématiquement l'absence de tout secret.
- Publication publique autorisée par le propriétaire ; cette migration d'identité ne modifie aucun dépôt privé.
