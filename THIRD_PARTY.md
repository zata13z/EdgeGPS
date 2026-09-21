# Composants et données tiers

La licence Apache 2.0 du code original ne remplace pas les licences ci-dessous.
Les bibliothèques ne sont pas vendoriées et aucun firmware compilé n'est livré.

| Composant | Version testée | Licence / provenance |
| --- | --- | --- |
| Arduino-ESP32 | 3.3.12 | LGPL-2.1-or-later vérifiée dans cores/esp32/Arduino.h ; composants ESP-IDF et bibliothèques avec leurs propres notices, pas de requalification globale sous Apache |
| TinyGPSPlus | 1.0.3 | LGPL-2.1-or-later, déclaration vérifiée dans TinyGPS++.h ; https://github.com/mikalhart/TinyGPSPlus |
| U8g2 | 2.36.19 | BSD-2-Clause déclarée dans library.properties ; notices propres aux polices à conserver ; https://github.com/olikraus/u8g2 |

Avant de distribuer un firmware binaire, vérifier les obligations des composants
effectivement liés, notamment les sources et possibilités de reliaison LGPL.
Cette préparation ne constitue pas une validation juridique de distribution binaire.

## Données géographiques

Les en-têtes communes95.h, communesFR.h et polygones95.h indiquent une provenance
API Geo / IGN / INSEE. france.h contient un contour dérivé des centres de communes.
Les noms de communes et leurs coordonnées sont des données géographiques publiques,
pas des parcours personnels enregistrés par l'appareil.

Sources : https://geo.api.gouv.fr/decoupage-administratif et
https://www.data.gouv.fr/datasets/decoupage-administratif-1

Le jeu administratif de référence est annoncé sous Licence Ouverte 2.0.
Cependant, le millésime exact et la traçabilité des contours embarqués ne sont pas
fournis dans les sources locales. Confirmer leurs conditions et la date des données
avant redistribution ; ne pas présenter ces données comme exclusivement Apache 2.0.
Les scripts de génération d'origine ne sont pas fournis : les en-têtes sont inclus
pour permettre la compilation sans téléchargement de données.
