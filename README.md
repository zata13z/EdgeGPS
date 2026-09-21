# EdgeGPS

Projet ZataLabs — ESP32-S3, récepteur u-blox M8N et écran OLED 128×64.
Code original sous [Apache License 2.0](LICENSE). Dépôt public.

## Connexion immédiate au Wi-Fi de la carte

| Paramètre | Valeur par défaut |
| --- | --- |
| Réseau Wi-Fi (SSID) | `EdgeGPS` |
| Mot de passe AP | `0SPGegdE0` |
| Interface web | http://192.168.4.1/ |
| Canal Wi-Fi | 6 |

Le premier et le dernier caractère du mot de passe sont le chiffre **zéro**.
Ce mot de passe appartient au **point d'accès créé par l'ESP32**, pas à une box personnelle.
Il est volontairement livré avec le projet, à la demande de son propriétaire.
Le dépôt compile directement : aucun fichier secret n'est requis pour l'usage standard.
La carte n'est pas un routeur Internet. Sur le téléphone, accepter « rester connecté
sans Internet » et saisir l'adresse en **HTTP**, pas en HTTPS. L'USB ne donne pas
automatiquement accès au serveur web : il faut rejoindre le Wi-Fi de la carte.

## Guide de lecture

- Pour utiliser l'appareil : Connexion immédiate, Matériel, Installation, Utilisation.
- Pour comprendre le firmware : Architecture détaillée et cycle d'exécution, GPS,
  géographie, sorties, stockage, OLED et réseau, ci-dessous.
- Pour modifier le projet : Réglages, API HTTP, interface JavaScript et tests.
- Pour redistribuer : Confidentialité, limites, licence et THIRD_PARTY.md.

## Fonctionnalités

- GPS sur UART, données NMEA et UBX ; recherche de commune embarquée.
- OLED autonome : position, vitesse/altitude/cap, sortie, connexion ; rotation 5 s.
- Interface web locale sans ressources distantes obligatoires.
- Démarrage, pause/reprise et fin de sortie ; historique LittleFS et export GPX.
- Sauvegarde périodique 10 s, récupération en pause au démarrage.
- Simulation fictive autour de Cergy ; profils piéton, vélo et automobile.

## Matériel et brochage

| Signal | ESP32-S3 |
| --- | --- |
| TXD du GPS → RX ESP32 | GPIO 4 |
| RXD du GPS ← TX ESP32 | GPIO 5 |
| OLED SDA | GPIO 8 |
| OLED SCL | GPIO 9 |
| Masse | GND commun |

Montage testé : ESP32-S3, flash 4 Mo, SSD1306 I2C à 0x3C, logique 3,3 V.
Vérifier l'alimentation admissible du module GPS exact, distincte du niveau logique.
UART initial 9 600 bauds, configuré à 38 400. PSRAM désactivée dans la configuration testée.
Ne pas utiliser ce prototype comme équipement de navigation de sécurité.

## Architecture

`EdgeGPS/EdgeGPS.ino` : acquisition, réseau HTTP, OLED, métriques et commandes.
`page.h` : HTML/CSS/JavaScript embarqués. `sorties.h` : journal LittleFS et GPX.
Les quatre en-têtes géographiques contiennent les données nécessaires à la compilation.
`config.example.h` permet une surcharge locale facultative ; `config.h` reste ignoré par Git.

## Installation reproductible

Installer Arduino CLI, puis, depuis la racine du dépôt :

```sh
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.12 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib install "TinyGPSPlus@1.0.3" "U8g2@2.36.19"
```

Par défaut, ne créer aucun fichier de configuration : le mot de passe AP est déjà
défini dans le sketch. Pour le personnaliser uniquement sur votre appareil, copier
`EdgeGPS/config.example.h` vers `EdgeGPS/config.h`, puis remplacer la valeur fictive
`EDGEGPS_AP_PASS` par 8 à 63 caractères ASCII. Ce fichier est ignoré par Git.
Si vous utilisez cette surcharge, le mot de passe à saisir sera celui de config.h,
et non celui du tableau ci-dessus. Ne pas copier l'exemple sans changer sa valeur.

```sh
arduino-cli compile --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=4M,PSRAM=disabled,PartitionScheme=huge_app" EdgeGPS
arduino-cli board list
```

Téléverser en remplaçant PORT par le port détecté :

```sh
arduino-cli upload -p PORT --fqbn "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashSize=4M,PSRAM=disabled,PartitionScheme=huge_app" EdgeGPS
```

Sous Arduino IDE : ouvrir `EdgeGPS/EdgeGPS.ino`, sélectionner ESP32S3 Dev Module,
USB CDC activé, USB Hardware CDC/JTAG, flash 4 Mo, Huge APP, PSRAM désactivée.
Ne pas changer de partition sur une carte contenant des sorties sans les exporter.

## Utilisation

1. Alimenter le montage et dégager la vue du ciel. L'OLED fonctionne sans téléphone.
2. Rejoindre le Wi-Fi `EdgeGPS` avec `0SPGegdE0` (sauf surcharge locale).
3. Ouvrir `http://192.168.4.1/` ; conserver cette connexion même sans Internet.
4. Attendre le fix puis utiliser Démarrer, Pause/Reprendre et Terminer.
5. Exporter le GPX depuis Historique. La suppression d'une sortie est définitive.

La connexion à une box est facultative, via Réseau. L'adresse apparaît sur l'OLED.
Terminer la sortie avant de couper l'alimentation. Les données non encore sauvegardées
peuvent être perdues lors d'une coupure. Le démarrage d'une sortie nécessite l'interface
web ; l'affichage OLED seul ne démarre pas un enregistrement.

Console USB à 115 200 bauds : `i` état réseau/stockage, `s` simulation, `r` remise à
zéro de l'aperçu hors sortie active. `o` efface les identifiants de box : commande destructive.

## Confidentialité et sécurité

- Le mot de passe AP livré est une exception explicite et volontaire : il permet
  l'utilisation du projet. Il n'est pas confidentiel pour les lecteurs du dépôt.
- Ne pas ajouter d'identifiant de box personnelle, token, clé privée, dump flash ou GPX.
- Les identifiants de box et les parcours restent dans la mémoire de la carte ; ce
  paquet de sources ne les exporte pas et ne les efface pas.
- HTTP local non chiffré, sans authentification applicative : toute personne ayant
  accès au réseau peut lire les positions et actionner les commandes. Réseau de confiance uniquement.
- Ne pas exposer le serveur à Internet. La confidentialité du dépôt ne sécurise pas l'appareil.
- Le lien OpenStreetMap est externe et transmet les coordonnées lors de son ouverture.
- Pour un usage hors environnement de confiance, personnaliser le mot de passe AP.

## Dépannage et limites

- GPS muet : contrôler masse commune et croisement TX/RX. Pas de fix : essayer dehors.
- OLED absent : contrôler I2C et type SSD1306/SH1106 (`USE_SSD1306`).
- Stockage : environ 896 Kio de partition, capacité finie ; exporter puis supprimer
  les anciennes sorties. Aucun formatage automatique d'une partition non vierge.
- Aperçu RAM limité à 600 points ; exporter les archives pour le parcours journalisé.
- Dénivelé GPS bruité, pas de baromètre ; dérive GPS possible à l'arrêt.
- Coupures en cours d'écriture et saturation nécessitent encore des essais terrain.
- Dates/millésimes des bases embarquées et régénération non reproductibles à ce stade.
- Aucune garantie d'absence de vulnérabilité : voir [AUDIT.md](AUDIT.md).

## Architecture détaillée et cycle d'exécution

Le projet est un sketch Arduino monolithique avec des en-têtes spécialisés. Il
n'utilise pas de serveur distant, de base de données externe ou de framework web.
`sorties.h` est inclus après les variables et fonctions qu'il utilise ; ce n'est
pas une bibliothèque autonome. Ne pas déplacer son inclusion sans revoir ces dépendances.

| Fichier | Rôle dans le programme |
| --- | --- |
| `EdgeGPS.ino` | Point d'entrée, périphériques, GPS, géographie, statistiques, OLED, serveur HTTP |
| `page.h` | Page HTML complète, styles adaptatifs et logique JavaScript ; chaîne PROGMEM |
| `sorties.h` | Journal binaire, reprise, historique, export et suppression de sorties |
| `communes95.h` | Centres des 183 communes du Val-d'Oise |
| `polygones95.h` | Contours simplifiés et boîtes englobantes pour l'appartenance communale |
| `communesFR.h` | 34 969 centres de communes triés par latitude |
| `france.h` | Contour simplifié utilisé par le dessin de la carte de France |
| `config.example.h` | Exemple de personnalisation locale, facultatif |
| `tests/interface.cjs` | Tests JavaScript sans carte, DOM simulé |

### Au démarrage : `setup()`

1. Initialise la console USB et l'I2C, recherche l'écran et initialise U8g2.
2. Ouvre les préférences NVS dans l'espace `edgegps` et appelle `initSorties()`.
3. Recharge le profil GPS et les éventuels identifiants de box mémorisés.
4. Démarre le point d'accès, puis le serveur HTTP : l'ordre évite d'ouvrir une
   socket avant l'initialisation du réseau.
5. Si une box est mémorisée, tente de la rejoindre sans bloquer tout le programme.
6. Initialise l'UART GPS, envoie la configuration M8N et démarre le chronométrage.

### En fonctionnement : `loop()`

La boucle est coopérative : aucun thread applicatif séparé n'est créé pour le GPS
ou le serveur. À chaque passage, elle traite HTTP, console, état Wi-Fi et relances
UBX ; actualise la durée de sortie et la sauvegarde ; injecte éventuellement la
simulation ; lit l'UART ; actualise satellites, commune et trace ; puis rafraîchit
l'écran si son échéance est atteinte.

La lecture UART est limitée à environ 2 ms par passage pour rendre la main aux
autres tâches. Le tampon RX de 16 Kio absorbe les rafales GPS. L'OLED est rafraîchi
à 2 Hz, les pages changent toutes les 5 s. Les logs USB sont espacés de 2 s et
conditionnés à la disponibilité de la sortie pour ne pas bloquer l'usage sur batterie.
Ces précautions ne rendent pas le serveur asynchrone : une longue réponse HTTP
peut encore retarder les autres traitements.

## Comment le code traite le GPS

### Deux protocoles complémentaires

`gps.encode()` de TinyGPSPlus lit les phrases NMEA : latitude, longitude, altitude,
vitesse, cap, date, heure et indicateurs de qualité. `decoderUBX()` analyse en
parallèle les paquets binaires u-blox : synchronisation B5 62, classe, identifiant,
longueur, données et deux octets de contrôle. Les paquets trop grands ou dont la
somme de contrôle ne correspond pas sont rejetés.

`traiterUBX()` interprète les messages reconnus :

| Message | Informations exploitées |
| --- | --- |
| NAV-PVT | Type et validité du fix, satellites utilisés, précisions horizontale/verticale |
| NAV-SAT | Satellites visibles, constellation, azimut, élévation et signal |
| NAV-ODO | Distance de l'odomètre intégré et précision associée |
| MON-VER | Versions logicielles et matérielles du module |
| ACK-NAK | Refus d'une commande, signalé sur la console |

`envoyerUBX()` construit les commandes et leurs contrôles. `configurerM8N()` demande
38 400 bauds, une solution de navigation toutes les 200 ms, NAV-PVT à 5 Hz et NAV-SAT
à 1 Hz. `surveillerUBX()` relance l'identification et certaines cadences jusqu'à
quatre fois. La vitesse NMEA effectivement reçue dépend aussi de la configuration
du module. Le code ne balaie pas automatiquement tous les débits UART possibles.

`fixActuel()` utilise la validité NAV-PVT et un âge maximal de 3 s lorsque l'UBX est
détecté, sinon la validité et l'âge NMEA. En simulation, le critère NMEA est utilisé.
Les coordonnées restent issues de TinyGPSPlus : le code ne remplace pas la latitude
NMEA par celle du paquet NAV-PVT. La perte de NAV-PVT après détection UBX peut donc
faire afficher « pas de fix » même si des phrases NMEA continuent d'arriver.

`majSatellites()` exploite les champs GPGSV en repli NMEA ; avec UBX, les informations
proviennent de NAV-SAT. Le tableau d'affichage est limité à 32 satellites.

### Profils et simulation

`appliquerProfilM8()` configure NAV5 et l'odomètre : profil interne 0 pour piéton,
1 pour vélo, 3 pour automobile. Le modèle dynamique NAV5 est pedestrian pour piéton
et vélo, automotive pour voiture : il ne s'agit pas de trois algorithmes GPS distincts.
La distance de sortie calculée par l'ESP32 reste distincte de l'odomètre M8N affiché.

`simuler()` fabrique une boucle fictive autour de Cergy. `versNMEA()` formate les
coordonnées et `injecterNMEA()` ajoute le checksum puis alimente TinyGPSPlus.
L'injection a lieu toutes les 500 ms. Les NMEA réels ne sont alors pas injectés dans
TinyGPSPlus, mais le décodeur UBX continue de fonctionner : certains diagnostics UBX
peuvent encore décrire le récepteur réel. Les archives de simulation sont étiquetées.
La date simulée est fixe dans le code ; ce n'est pas une mesure réelle.

## Localisation dans les communes

`distanceM()` utilise une approximation plane avec correction de longitude par le
cosinus de la latitude. Elle convient à l'usage local mais ne constitue pas une
géodésie de précision mondiale.

`chercherCommune()` commence par les contours du Val-d'Oise : les boîtes englobantes
éliminent les candidats éloignés, puis `dansPolygone()` réalise un test par lancer
de rayon. `communeExacte=true` signifie « intérieur d'un contour embarqué », sans
garantie cadastrale compte tenu des simplifications.

À défaut, `plusProcheFR()` trouve un point d'entrée par recherche dichotomique dans
la table triée, puis explore les latitudes voisines jusqu'à pouvoir arrêter la
recherche. Il choisit le centre le plus proche selon sa métrique, pas nécessairement
la commune contenant réellement le point. Au-delà de 25 km du centre trouvé, le
nom n'est plus retenu. Le seuil de 8 km `DIST_HORS_DEPT_M` subsiste mais n'est plus
utilisé pour cette sélection nationale.

## Sorties : états, calculs et sauvegarde

### Cycle d'une sortie

`handleSortieDebut()` refuse un départ si une sortie existe déjà, si le stockage
est indisponible ou si l'espace libre est inférieur à 32 Kio. Il remet l'aperçu à
zéro, alloue un identifiant, écrit un checkpoint initial et mémorise `activeTrip`.

`majTrace()` ne travaille que lorsque la sortie est active et non en pause. Un
point est ajouté au premier fix, après au moins 8 m depuis le dernier point retenu,
ou après 10 s. Ces conditions sont alternatives, pas cumulatives. Sans fix valide,
aucun nouveau point n'est ajouté, mais la durée continue si la sortie n'est pas en pause.

`handleTracePause()` bascule pause/reprise, sauvegarde un checkpoint et réinitialise
la référence de distance et de dénivelé. La reprise incrémente le numéro de segment.
Le trajet effectué pendant la pause n'est donc pas ajouté à la distance suivante.
`handleSortieFin()` écrit un marqueur de fin puis retire `activeTrip` des préférences.

### Métriques

| Valeur | Calcul / unité |
| --- | --- |
| Distance | Somme des distances entre points retenus, en mètres |
| Durée | Millisecondes écoulées hors pause ; exposées en secondes par l'API |
| Moyenne | `distanceTotale * 3600 / traceDureeMs`, résultat en km/h |
| Maximum | Plus grande vitesse NMEA observée pendant l'enregistrement |
| Altitude min/max | Extrêmes observés pendant la sortie |
| D+ / D− | Somme des écarts d'altitude d'au moins 2 m entre points successifs |

Exemple : 1 000 m en 360 000 ms donne 10 km/h. Le filtre de dénivelé compare deux
points successifs : de petites variations répétées peuvent être ignorées. Sans
filtrage avancé de l'immobilité, la dérive GPS peut ajouter de la distance à l'arrêt.

### RAM, NVS et LittleFS : trois rôles distincts

- `trace[]` est un anneau RAM de 600 `Point` : les nouveaux points remplacent les
  plus anciens dans l'aperçu. Un point contient latitude, longitude, altitude,
  vitesse quantifiée et date Unix produite par `versEpoch()`.
- `Preferences` mémorise notamment `profil`, `ssid`, `pass`, `activeTrip` et `tripNext`.
- LittleFS conserve `/sortie-ID.bin` avec les points de toute la sortie tant que
  l'espace le permet. Ces fichiers ne sont pas des fichiers du dépôt Git.

`Enregistrement` contient un marqueur de format, les métriques, un point, un numéro
de segment, un type (1 point, 2 checkpoint, 3 fin), un indicateur de simulation et
un checksum FNV. Ce checksum détecte certaines corruptions ; il ne chiffre ni
n'authentifie les données. Le format écrit directement une structure C++ : ne pas
supposer sa compatibilité avec un autre compilateur ou une modification de structure.

`journalPoint()` remplit un tampon de 64 enregistrements. `sauverJournal()` l'ajoute
au fichier et ferme celui-ci ; `serviceSorties()` écrit un checkpoint toutes les
10 s pendant l'enregistrement. Pause et fin déclenchent également une sauvegarde.
Sous 32 Kio libres, l'enregistrement se suspend pour préserver une marge de stockage.

`initSorties()` monte LittleFS sans effacement automatique. Si le montage échoue,
le formatage n'est tenté que si toute la partition est vierge (octets FF). Une
sortie interrompue est relue, contrôlée et restaurée **en pause**, avec les derniers
600 points dans l'aperçu. Un journal invalide provoque une erreur, pas une réparation
automatique. Les dernières données non sauvegardées peuvent être perdues à la coupure.

### Deux exports différents

`/track.gpx` exporte uniquement l'aperçu RAM, limité à 600 points. `/sortie.gpx?id=ID`
relit le journal conservé et sépare les reprises par des éléments GPX `trkseg`.
Pour conserver une sortie entière, utiliser **Historique → GPX**, de préférence
après Terminer. L'export d'une sortie active ne comprend que les données déjà écrites.
Le dessin de l'aperçu RAM ne conserve pas les numéros de segment du journal.

## OLED et réseau

`contenu()` choisit la page avec `millis()`, sans attente bloquante : position,
GPS, sortie, puis connexion. `bandeau()` dessine satellites et heure UTC.
`drawTexteAdapte()` réduit la police pour les textes longs. `drawScreen()` prépare
le framebuffer complet de 1 024 octets puis l'envoie par I2C. `ecranAttente()` a
gardé son ancien nom, mais est maintenant la dernière page de connexion et ne
bloque plus le GPS quand aucun téléphone n'est présent.

`demarrerAP()` crée le réseau EdgeGPS. `tenterSTA()` passe temporairement en AP+STA
pour rejoindre une box tout en laissant l'AP accessible. `surveillerReseau()` gère
le délai de 45 s : en cas d'échec, retour AP ; en cas de succès, mémorisation des
identifiants et arrêt de l'AP après un sursis de 3 s. Après cette bascule, rejoindre
le réseau de la box et utiliser l'adresse affichée sur l'OLED. Si la box est perdue,
le programme revient en AP. `clientPresent()` combine activité HTTP récente et
stations associées ; il n'intervient plus dans le choix des pages GPS.

## API HTTP : routes et effets

Les lectures sont utilisées en GET ; les actions ci-dessous sont enregistrées en
POST dans `demarrerServeur()`. Pas de session ni d'authentification applicative.

| Méthode | Route | Rôle / paramètres |
| --- | --- | --- |
| GET | `/` | Page embarquée, sans cache |
| GET | `/api` | Position, fix, métriques, diagnostics, mémoire libre |
| GET | `/sats` | Liste des satellites pour ciel et barres de signal |
| GET | `/proches` | Communes proches de la position |
| GET | `/france.json` | Contour dessiné par le navigateur |
| GET | `/track.json` | Points de l'aperçu RAM |
| GET | `/track.gpx` | Export GPX de l'aperçu RAM |
| GET | `/sortie/etat` | active, pause, id, stockage, erreur et octets libres |
| POST | `/sortie/debut` | Démarrer une nouvelle sortie |
| POST | `/trace/pause` | Basculer pause/reprise ; action non idempotente |
| POST | `/sortie/fin` | Terminer et sauvegarder |
| GET | `/historique` | Résumés des journaux conservés |
| GET | `/sortie.gpx?id=ID` | Export d'une archive |
| POST | `/sortie/supprimer?id=ID` | Supprimer un journal, sauf la sortie active |
| POST | `/reset` | Réinitialiser aperçu/statistiques hors sortie active |
| POST | `/simu` | Basculer simulation ; `on=1`/`on=0` facultatif |
| POST | `/profil` | Champ formulaire `p=pieton`, `velo` ou `auto` |
| POST | `/wifi` | Champs formulaire `ssid` et `pass` pour la box |
| GET | `/wifi/etat` | État réseau et adresse, sans mot de passe de box |
| POST | `/wifi/oubli` | Oublier les identifiants de box et revenir en AP |

Les formulaires utilisent `application/x-www-form-urlencoded`. Les principales
erreurs sont 400 (paramètre invalide), 404 (archive absente), 409 (état incompatible)
et 507 (sauvegarde/espace insuffisant). Une route inconnue renvoie actuellement la
page d'accueil : ce n'est pas une API REST stricte. En cas de délai dépassé sur une
action, relire l'état avant de réessayer, surtout pour la bascule pause/reprise.

Dans `/api`, `dtot`, `dist`, `alt`, `hacc`, `vacc`, `dpos` et `dneg` sont en mètres ;
`spd`, `vmax`, `vmoy` en km/h ; `cap` en degrés ; `duree`, `up`, `ttff` en secondes ;
`heap` en octets. `hdop` est sans unité. `pts` compte l'aperçu, pas toute l'archive.
Vérifier `fix` avant d'utiliser les coordonnées ; les zéros ne signifient pas
automatiquement une position valide au point 0°, 0°.

## Fonctionnement de l'interface web

`requete()` encapsule fetch, désactive le cache, limite l'attente à 5 s et remonte
les erreurs JSON. `tick()` lit `/api`, appelle `maj()` puis actualise l'onglet
visible ; la prochaine itération est programmée 1 s après la fin du traitement.
Cela évite l'empilement de requêtes périodiques quand la carte répond lentement.
Le rafraîchissement est suspendu lorsque la page n'est plus visible.

`afficherSortie()` règle les boutons selon l'état reçu. `actionSortie()` bloque les
actions concurrentes depuis l'écran Sortie et relit l'état après une commande.
`afficherHistorique()` construit les éléments avec `textContent` et demande une
confirmation avant suppression ; l'historique est actualisé au plus toutes les 10 s
lorsqu'il reste ouvert. `rafraichirOnglet()` ne charge que les données utiles à
l'onglet, avec un verrou empêchant deux rafraîchissements d'onglet simultanés.

`dessinerCiel()` et `dessinerBarres()` tracent les satellites sur canvas.
`dessinerTrace()` adapte les points de l'aperçu à un canvas sans fond de carte.
`dessinerFrance()` utilise le contour embarqué. `majProches()` met à jour les
communes, `majReseau()` les informations de connexion. Le navigateur ne stocke
pas l'historique à la place de l'ESP32. OpenStreetMap reste un lien externe facultatif.

## Réglages et diagnostic pour les développeurs

| Réglage | Emplacement / effet |
| --- | --- |
| `AP_SSID`, `EDGEGPS_AP_PASS`, `AP_CANAL` | Nom, mot de passe et canal AP dans le sketch |
| `PIN_GPS_RX`, `PIN_GPS_TX` | UART : toujours considérer les directions côté ESP32 |
| `PIN_I2C_SDA`, `PIN_I2C_SCL` | Brochage OLED |
| `GPS_BAUD_USINE`, `GPS_BAUD` | Débit au démarrage puis après configuration |
| `USE_SSD1306` | 1 pour SSD1306, 0 pour SH1106 |
| `FIX_MAX_AGE_MS` | Âge maximal d'un fix, actuellement 3 000 ms |
| `TRACE_MAX` | Taille de l'anneau RAM ; ne change pas la capacité flash |
| `TRACE_DIST_M`, `TRACE_PERIODE` | Critères d'ajout d'un point |
| `ECHO_NMEA` | Copie brute UART vers USB ; désactivée pour les performances |

Commandes console supplémentaires : `b` mesure la recherche de communes sur des
points fictifs ; `t` teste le retour AP via un réseau volontairement inexistant
et perturbe donc la connexion courante. Les lettres majuscules sont acceptées.
`s` et `r` sont ignorées pendant une sortie active. Aucun bouton matériel de
démarrage, OTA, carte SD ou indicateur de batterie n'est implémenté.

La partition `huge_app` réserve 3 Mio à l'application et environ 896 Kio au stockage.
La RAM affichée par la compilation concerne les variables globales, pas toute la
mémoire utilisée à l'exécution : Wi-Fi, piles et chaînes dynamiques s'y ajoutent.
L'absence de PSRAM activée n'empêche pas ce programme de fonctionner.

## Tests et publication GitHub

Test de l'interface (Node.js installé séparément, sans paquet npm) :

```sh
node tests/interface.cjs
```

Dépôt **public** : https://github.com/ZataLabs/EdgeGPS.
Les réserves de [THIRD_PARTY.md](THIRD_PARTY.md) restent applicables avant redistribution.
Inspecter `git status` et le contenu à ajouter avant tout commit. Ne pas forcer l'ajout
d'un fichier ignoré. Les commits de publication utilisent ZataLabs et l'adresse GitHub noreply.

## Licence

Copyright 2026 ZataLabs. Code original : Apache License 2.0, texte officiel dans
[LICENSE](LICENSE), attribution dans [NOTICE](NOTICE). Cette licence autorise notamment
l'usage commercial selon ses conditions. Les données et dépendances restent régies par
leurs propres licences : [THIRD_PARTY.md](THIRD_PARTY.md).

Texte de licence téléchargé sans modification depuis
https://www.apache.org/licenses/LICENSE-2.0.txt.
