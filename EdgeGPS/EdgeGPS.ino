/*
 * EdgeGPS — ZataLabs. Copyright 2026 ZataLabs.
 * Code original sous Apache-2.0 ; voir LICENSE et THIRD_PARTY.md.
 * ESP32-S3 Super Mini + u-blox NEO-M8N + OLED I2C 0.96" (SSD1306)
 * Traceur GPS Val-d'Oise : commune, satellites, trace, statistiques,
 * point d'acces WiFi et page web embarquee.
 *
 * Carte    : "ESP32S3 Dev Module", USB CDC On Boot = Enabled
 * GPS      : UART1, GPS TXD -> IO4 (RX ESP32), GPS RXD -> IO5 (TX ESP32), 9600 bauds
 * OLED     : I2C, SDA -> IO8, SCL -> IO9
 * Libs     : TinyGPSPlus (Mikal Hart), U8g2 (olikraus)
 *
 * Endpoints : /            page web
 *             /api         etat courant (JSON)
 *             /sats        satellites en vue (JSON)
 *             /track.json  trace enregistree (JSON)
 *             /track.gpx   trace au format GPX (telechargement)
 *             /proches     les 6 communes les plus proches (JSON)
 *             /reset       efface la trace et les statistiques
 *             /simu?on=1   position simulee, pour essayer sans fix
 */

#include <Wire.h>
#include <U8g2lib.h>
#include <TinyGPSPlus.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <math.h>
#include <LittleFS.h>
#include "communes95.h"
#include "polygones95.h"
#include "communesFR.h"
#include "france.h"
#include "page.h"

// ---------- Reseau ----------
// Point d'acces de secours, toujours disponible au demarrage.
#define AP_SSID      "EdgeGPS"
#if __has_include("config.h")
#include "config.h" // Surcharge locale facultative, non versionnee.
#endif
#ifndef EDGEGPS_AP_PASS
// Mot de passe du point d'acces du projet, volontairement documente.
#define EDGEGPS_AP_PASS "0SPGegdE0"
#endif
#define AP_PASS EDGEGPS_AP_PASS
static_assert(sizeof(EDGEGPS_AP_PASS) >= 9 && sizeof(EDGEGPS_AP_PASS) <= 64,
              "Le mot de passe WiFi doit contenir entre 8 et 63 caracteres");
#define AP_CANAL     6

// Duree laissee a la connexion au routeur avant de rebasculer en point d'acces.
#define STA_TIMEOUT_MS   45000UL

// Un client est considere present tant qu'une requete HTTP a ete servie
// dans ce delai. La page interroge /api chaque seconde, donc 6 s suffisent
// et l'ecran ne clignote pas entre deux requetes.
#define CLIENT_TIMEOUT_MS 6000UL

// ---------- Brochage ----------
#define PIN_GPS_RX   4      // entree ESP32 <- TXD du GPS
#define PIN_GPS_TX   5      // sortie ESP32 -> RXD du GPS
#define PIN_I2C_SDA  8
#define PIN_I2C_SCL  9

#define GPS_BAUD_USINE 9600
#define GPS_BAUD       38400 // UBX + NMEA a 5 Hz
#define ECHO_NMEA    0      // 1 = recopie les trames brutes sur USB (diagnostic)
#define FIX_MAX_AGE_MS 3000UL // au-dela, la derniere position est consideree perimee

// Au-dela de cette distance au centre de commune le plus proche, on considere
// qu'on est sorti du departement. Verifie sur des points connus : a l'interieur
// du 95 on reste sous 2,5 km du centre le plus proche, alors que Paris centre
// tombe a 12,7 km de Montmagny. 8 km separe proprement les deux cas.
#define DIST_HORS_DEPT_M  8000.0f

// Au-dela, on est hors de France (mer, etranger). La commune francaise la
// plus eloignee de son propre centre reste tres en dessous de ce seuil.
#define DIST_HORS_FRANCE_M  25000.0f

// ---------- Trace ----------
#define TRACE_MAX      600   // points en tampon circulaire (~9,6 ko)
#define TRACE_DIST_M   8.0f  // distance mini entre deux points enregistres
#define TRACE_PERIODE  10000 // ms : on enregistre au moins un point par 10 s

// ---------- Ecran ----------
// 0.96" = SSD1306 ; 1.3" = SH1106. Ici : SSD1306 (ecran 0.96" confirme).
// Passer a 0 si on remet un 1.3" SH1106.
#define USE_SSD1306 1

#if USE_SSD1306
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#else
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#endif

TinyGPSPlus gps;
WebServer   server(80);
Preferences prefs;

// ---------- Etat reseau ----------
enum ModeReseau { MODE_AP, MODE_STA_ESSAI, MODE_STA };
static ModeReseau modeReseau     = MODE_AP;
static uint32_t   staDebut       = 0;      // millis au lancement de la tentative
static uint32_t   staSucces      = 0;      // millis a la connexion, pour la grace
static String     staSsid, staPass;
static String     staIp;
static uint32_t   dernierClient  = 0;      // millis de la derniere requete servie
static String     dernierEchec;            // motif du dernier echec, pour la page

// ---------- Satellites ----------
// TinyGPSPlus ne decode pas les GSV : on extrait les champs a la main.
// Une GSV decrit jusqu'a 4 satellites, champs 4..7, 8..11, 12..15, 16..19.
#define SAT_MAX 32
struct Satellite { uint8_t gnss, prn, snr; int8_t elev; int16_t azim; bool actif; };
static Satellite sats[SAT_MAX];
static uint8_t   satsEnVue = 0;

// ---------- Protocole binaire u-blox ----------
// NAV-PVT apporte le type de fix et les precisions, NAV-SAT les constellations.
struct UbxEtat {
  bool detecte, pvtValide;
  uint8_t fixType, nbUtilises;
  float hAcc, vAcc;
  uint32_t dernierPvt;
  char logiciel[31], materiel[11];
  uint32_t odoDistance, odoTotal, odoPrecision;
};
static UbxEtat ubx = {};
static uint8_t ubxPayload[512];
static uint16_t ubxLongueur = 0, ubxPos = 0;
static uint8_t ubxClasse = 0, ubxId = 0, ubxCkA = 0, ubxCkB = 0, ubxEtatParse = 0;
static uint32_t ubxSync = 0, ubxTrames = 0, ubxCkErreur = 0;

TinyGPSCustom gsvTotal(gps, "GPGSV", 1);
TinyGPSCustom gsvIndex(gps, "GPGSV", 2);
TinyGPSCustom gsvEnVue(gps, "GPGSV", 3);
TinyGPSCustom gsvPrn[4]  = { {gps,"GPGSV",4},  {gps,"GPGSV",8},  {gps,"GPGSV",12}, {gps,"GPGSV",16} };
TinyGPSCustom gsvElev[4] = { {gps,"GPGSV",5},  {gps,"GPGSV",9},  {gps,"GPGSV",13}, {gps,"GPGSV",17} };
TinyGPSCustom gsvAzim[4] = { {gps,"GPGSV",6},  {gps,"GPGSV",10}, {gps,"GPGSV",14}, {gps,"GPGSV",18} };
TinyGPSCustom gsvSnr[4]  = { {gps,"GPGSV",7},  {gps,"GPGSV",11}, {gps,"GPGSV",15}, {gps,"GPGSV",19} };
TinyGPSCustom gsaFixGps(gps, "GPGSA", 2);
TinyGPSCustom gsaFixGn (gps, "GNGSA", 2);

// ---------- Trace enregistree ----------
struct Point { float lat, lon; int16_t alt; uint8_t spd; uint32_t epoch; };
static Point    trace[TRACE_MAX];
static uint16_t traceDebut = 0;    // index du plus ancien point
static uint16_t traceNb    = 0;    // nombre de points valides

// ---------- Statistiques ----------
static float    distanceTotale = 0;    // metres
static float    vitesseMax     = 0;    // km/h
static float    altMin = 0, altMax = 0;
static bool     altInit        = false;
static uint32_t ttff           = 0;    // ms jusqu'au premier fix
static uint32_t tDepart        = 0;

// ---------- Etat ----------
static uint32_t lastDraw = 0;
static uint32_t lastTrace = 0;
static uint32_t charsAtLastCheck = 0;
static bool     simuActive = false;
static float    traceDernLat = 0, traceDernLon = 0;
static bool     tracePremier = true;
static bool     tracePause = false;
static bool sortieActive = false;
static uint32_t sortieId = 0;
static uint16_t sortieSegment = 0;
void journalPoint(const Point& p);
bool sauverJournal();
static uint32_t traceDureeMs = 0, traceDernTick = 0;
static float    denivelePos = 0, deniveleNeg = 0;
static int16_t  traceDernAlt = 0;
static bool     traceAltInit = false;
static uint8_t  profilM8 = 3; // 0 course/pieton, 1 velo, 3 automobile

static const char* communeNom    = nullptr;
static float       communeDist   = -1.0f;  // -1 = pas encore cherche
static bool        communeExacte = false;  // true = point dans le contour

bool fixActuel() {
  if (ubx.detecte && !simuActive)
    return ubx.pvtValide && millis() - ubx.dernierPvt <= FIX_MAX_AGE_MS;
  return gps.location.isValid() && gps.location.age() <= FIX_MAX_AGE_MS;
}

uint16_t lireU16(const uint8_t* p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
uint32_t lireU32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                                           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

void envoyerUBX(uint8_t classe, uint8_t id, const uint8_t* p, uint16_t n) {
  uint8_t a = 0, b = 0;
  auto octet = [&](uint8_t v) { Serial1.write(v); a += v; b += a; };
  Serial1.write(0xB5); Serial1.write(0x62);
  octet(classe); octet(id); octet(n & 255); octet(n >> 8);
  for (uint16_t i = 0; i < n; i++) octet(p[i]);
  Serial1.write(a); Serial1.write(b); Serial1.flush();
}

void traiterUBX() {
  ubxTrames++;
  if (ubxClasse == 0x05 && ubxId == 0x00 && ubxLongueur >= 2)
    Serial.printf("UBX NAK pour classe 0x%02X message 0x%02X\n", ubxPayload[0], ubxPayload[1]);
  ubx.detecte = true;
  if (ubxClasse == 0x01 && ubxId == 0x07 && ubxLongueur >= 92) { // NAV-PVT
    ubx.fixType = ubxPayload[20];
    ubx.nbUtilises = ubxPayload[23];
    ubx.hAcc = lireU32(ubxPayload + 40) / 1000.0f;
    ubx.vAcc = lireU32(ubxPayload + 44) / 1000.0f;
    ubx.pvtValide = ubx.fixType >= 2 && (ubxPayload[21] & 0x01);
    ubx.dernierPvt = millis();
  } else if (ubxClasse == 0x01 && ubxId == 0x35 && ubxLongueur >= 8) { // NAV-SAT
    uint8_t n = min((uint8_t)SAT_MAX, ubxPayload[5]);
    for (uint8_t i = 0; i < SAT_MAX; i++) sats[i].actif = false;
    satsEnVue = n;
    for (uint8_t i = 0; i < n && 8 + i * 12 + 11 < ubxLongueur; i++) {
      const uint8_t* s = ubxPayload + 8 + i * 12;
      sats[i].gnss = s[0]; sats[i].prn = s[1]; sats[i].snr = s[2];
      sats[i].elev = (int8_t)s[3]; sats[i].azim = (int16_t)lireU16(s + 4);
      sats[i].actif = true;
    }
  } else if (ubxClasse == 0x01 && ubxId == 0x09 && ubxLongueur >= 20) { // NAV-ODO
    ubx.odoDistance = lireU32(ubxPayload + 8);
    ubx.odoTotal = lireU32(ubxPayload + 12);
    ubx.odoPrecision = lireU32(ubxPayload + 16);
  } else if (ubxClasse == 0x0A && ubxId == 0x04 && ubxLongueur >= 40) { // MON-VER
    memcpy(ubx.logiciel, ubxPayload, 30); ubx.logiciel[30] = 0;
    memcpy(ubx.materiel, ubxPayload + 30, 10); ubx.materiel[10] = 0;
    Serial.printf("u-blox detecte : %s / HW %s\n", ubx.logiciel, ubx.materiel);
  }
}

void appliquerProfilM8(uint8_t profil) {
  profilM8 = profil == 1 ? 1 : profil == 3 ? 3 : 0;
  uint8_t nav5[36] = {};
  nav5[0] = 0x01; // masque dynModel uniquement
  nav5[2] = profilM8 == 3 ? 4 : 3; // automotive ou pedestrian
  envoyerUBX(0x06,0x24,nav5,sizeof(nav5));

  uint8_t odo[20] = {};
  odo[4] = 0x01; // active l'odometre integre
  odo[5] = profilM8;
  envoyerUBX(0x06,0x1E,odo,sizeof(odo));
  const uint8_t msg[] = {0x01,0x09,5};
  envoyerUBX(0x06,0x01,msg,sizeof(msg)); // NAV-ODO a 1 Hz
}

void decoderUBX(uint8_t c) {
  switch (ubxEtatParse) {
    case 0: if (c == 0xB5) { ubxSync++; ubxEtatParse = 1; } break;
    case 1: ubxEtatParse = c == 0x62 ? 2 : (c == 0xB5 ? 1 : 0); ubxCkA = ubxCkB = 0; break;
    case 2: ubxClasse = c; ubxCkA += c; ubxCkB += ubxCkA; ubxEtatParse = 3; break;
    case 3: ubxId = c; ubxCkA += c; ubxCkB += ubxCkA; ubxEtatParse = 4; break;
    case 4: ubxLongueur = c; ubxCkA += c; ubxCkB += ubxCkA; ubxEtatParse = 5; break;
    case 5:
      ubxLongueur |= (uint16_t)c << 8; ubxCkA += c; ubxCkB += ubxCkA; ubxPos = 0;
      ubxEtatParse = ubxLongueur > sizeof(ubxPayload) ? 0 : (ubxLongueur ? 6 : 7); break;
    case 6:
      ubxPayload[ubxPos++] = c; ubxCkA += c; ubxCkB += ubxCkA;
      if (ubxPos == ubxLongueur) ubxEtatParse = 7; break;
    case 7: if (c == ubxCkA) ubxEtatParse = 8; else { ubxCkErreur++; ubxEtatParse = 0; } break;
    case 8: if (c == ubxCkB) traiterUBX(); else ubxCkErreur++; ubxEtatParse = 0; break;
  }
}

void configurerM8N() {
  // Passage de l'UART1 du module a 38400 bauds, protocoles UBX + NMEA.
  const uint8_t port[] = {1,0,0,0,0xD0,0x08,0,0,0x00,0x96,0,0,7,0,3,0,0,0,0,0};
  envoyerUBX(0x06, 0x00, port, sizeof(port));
  delay(120);
  Serial1.updateBaudRate(GPS_BAUD);
  delay(80);
  const uint8_t rate[] = {200,0,1,0,1,0}; // solution de navigation a 5 Hz
  envoyerUBX(0x06, 0x08, rate, sizeof(rate));
  // Format long : cadence explicite pour DDC, UART1, UART2, USB et SPI.
  const uint8_t pvt[] = {0x01,0x07,0,1,0,0,0,0}; envoyerUBX(0x06,0x01,pvt,sizeof(pvt));
  const uint8_t sat[] = {0x01,0x35,0,5,0,0,0,0}; envoyerUBX(0x06,0x01,sat,sizeof(sat));
  appliquerProfilM8(profilM8);
  envoyerUBX(0x0A, 0x04, nullptr, 0); // identification du module
}

void surveillerUBX() {
  static uint32_t derniere = 0;
  static uint8_t essais = 0;
  if (ubx.logiciel[0] || essais >= 4 || millis() - derniere < 2000) return;
  derniere = millis(); essais++;
  const uint8_t pvt[] = {0x01,0x07,0,1,0,0,0,0}; envoyerUBX(0x06,0x01,pvt,sizeof(pvt));
  const uint8_t sat[] = {0x01,0x35,0,5,0,0,0,0}; envoyerUBX(0x06,0x01,sat,sizeof(sat));
  envoyerUBX(0x0A,0x04,nullptr,0);
  Serial.printf("Interrogation UBX M8N (%u/4)\n", essais);
}

// ============================================================
//  Geodesie
// ============================================================

// Distance en metres entre deux points, approximation equirectangulaire.
// A l'echelle du departement l'erreur reste sous le metre.
float distanceM(float lat1, float lon1, float lat2, float lon2) {
  float k  = cosf(lat1 * 0.017453292f);
  float dy = (lat1 - lat2) * 110574.0f;
  float dx = (lon1 - lon2) * 111320.0f * k;
  return sqrtf(dx * dx + dy * dy);
}

// Appartenance a un polygone par lancer de rayon horizontal : on compte les
// aretes traversees a droite du point, un nombre impair signifie dedans.
bool dansPolygone(float lat, float lon, const CommunePoly& c) {
  // Le point est ramene dans le repere local de la commune, en pas de grille.
  float pu = (lat - c.baseLat) / POLY_SCALE;
  float pv = (lon - c.baseLon) / POLY_SCALE;

  bool dedans = false;
  uint16_t j = c.n - 1;
  for (uint16_t i = 0; i < c.n; j = i++) {
    const uint16_t* a = PVERTS[c.off + i];
    const uint16_t* b = PVERTS[c.off + j];
    if ((a[0] > pu) != (b[0] > pu)) {
      float x = (float)(b[1] - a[1]) * (pu - a[0]) / (float)((int32_t)b[0] - a[0]) + a[1];
      if (pv < x) dedans = !dedans;
    }
  }
  return dedans;
}

// Plus proche commune de France. La table FR est triee par latitude : on se
// positionne par dichotomie, puis on s'ecarte de part et d'autre en s'arretant
// des que le seul ecart de latitude depasse deja la meilleure distance trouvee.
// Le resultat est exact, et on n'examine typiquement que quelques dizaines
// d'entrees sur 34 969.
const CommuneFR* plusProcheFR(float lat, float lon, float* dist, uint32_t* examinees) {
  int32_t la = (int32_t)(lat * 1000000.0f);
  int32_t lo = (int32_t)(lon * 1000000.0f);

  uint32_t a = 0, b = NB_FR;
  while (a < b) { uint32_t m = (a + b) / 2; if (FR[m].lat < la) a = m + 1; else b = m; }

  const float k = cosf(lat * 0.017453292f);
  float best = 1e18f;
  const CommuneFR* trouve = nullptr;
  uint32_t n = 0;

  for (uint8_t sens = 0; sens < 2; sens++) {
    int32_t pas = sens ? -1 : 1;
    for (int32_t i = sens ? (int32_t)a - 1 : (int32_t)a;
         i >= 0 && i < (int32_t)NB_FR; i += pas) {
      float dy = (la - FR[i].lat) * 1e-6f * 110574.0f;
      if (dy * dy >= best) break;          // tri par latitude : inutile d'aller plus loin
      n++;
      float dx = (lo - FR[i].lon) * 1e-6f * 111320.0f * k;
      float d2 = dx * dx + dy * dy;
      if (d2 < best) { best = d2; trouve = &FR[i]; }
    }
  }
  *dist = sqrtf(best);
  if (examinees) *examinees = n;
  return trouve;
}

// Commune exacte par les contours, avec repli sur le centre le plus proche
// quand le point ne tombe dans aucun polygone (hors departement).
// Le filtre par boite englobante ecarte l'immense majorite des candidats
// en quatre comparaisons, avant tout calcul.
void chercherCommune(float lat, float lon) {
  for (uint16_t i = 0; i < NB_POLYS; i++) {
    const CommunePoly& c = POLYS[i];
    if (lat < c.baseLat || lat > c.maxLat || lon < c.baseLon || lon > c.maxLon) continue;
    if (!dansPolygone(lat, lon, c)) continue;

    communeNom    = c.nom;
    communeExacte = true;
    // Distance au centre, uniquement pour l'affichage.
    for (uint16_t k2 = 0; k2 < NB_COMMUNES; k2++)
      if (COMMUNES[k2].nom == c.nom || strcmp(COMMUNES[k2].nom, c.nom) == 0) {
        communeDist = distanceM(lat, lon, COMMUNES[k2].lat, COMMUNES[k2].lon);
        return;
      }
    communeDist = 0;
    return;
  }

  // Hors Val-d'Oise : on bascule sur les 34 969 communes de France, par
  // proximite du centre. Precis a quelques centaines de metres en zone dense,
  // a un a deux kilometres en zone rurale ou les communes sont vastes.
  float d;
  const CommuneFR* c = plusProcheFR(lat, lon, &d, nullptr);
  communeDist   = d;
  communeExacte = false;
  communeNom    = (c && d <= DIST_HORS_FRANCE_M) ? c->nom : nullptr;
}

// Jours ecoules depuis l'epoque Unix, algorithme de Howard Hinnant.
uint32_t versEpoch(uint16_t an, uint8_t mois, uint8_t jour,
                   uint8_t h, uint8_t m, uint8_t s) {
  an -= mois <= 2;
  int32_t era = (int32_t)an / 400;
  uint32_t yoe = an - era * 400;
  uint32_t doy = (153 * (mois + (mois > 2 ? -3 : 9)) + 2) / 5 + jour - 1;
  uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  int32_t jours = era * 146097 + (int32_t)doe - 719468;
  return (uint32_t)jours * 86400UL + h * 3600UL + m * 60UL + s;
}

// ============================================================
//  Satellites
// ============================================================

void majSatellites() {
  if (ubx.detecte) return; // NAV-SAT est plus complet que les seules trames GPGSV
  if (!gsvTotal.isUpdated()) return;

  uint8_t total = atoi(gsvTotal.value());
  uint8_t idx   = atoi(gsvIndex.value());
  satsEnVue     = atoi(gsvEnVue.value());
  if (idx == 0 || total == 0) return;

  // Premiere trame du cycle : on invalide tout avant de remplir.
  if (idx == 1)
    for (uint8_t i = 0; i < SAT_MAX; i++) sats[i].actif = false;

  for (uint8_t j = 0; j < 4; j++) {
    uint8_t prn = atoi(gsvPrn[j].value());
    if (prn == 0) continue;
    uint8_t slot = (idx - 1) * 4 + j;
    if (slot >= SAT_MAX) break;
    sats[slot].prn   = prn;
    sats[slot].elev  = atoi(gsvElev[j].value());
    sats[slot].azim  = atoi(gsvAzim[j].value());
    sats[slot].snr   = atoi(gsvSnr[j].value());   // vide => 0 = non capte
    sats[slot].actif = true;
  }
}

// ============================================================
//  Trace et statistiques
// ============================================================

void ajouterPoint(float lat, float lon) {
  uint16_t pos = (traceDebut + traceNb) % TRACE_MAX;
  if (traceNb == TRACE_MAX) {               // tampon plein : on ecrase le plus ancien
    pos = traceDebut;
    traceDebut = (traceDebut + 1) % TRACE_MAX;
  } else {
    traceNb++;
  }

  trace[pos].lat = lat;
  trace[pos].lon = lon;
  trace[pos].alt = gps.altitude.isValid() ? (int16_t)gps.altitude.meters() : 0;
  if (gps.altitude.isValid()) {
    int16_t a = trace[pos].alt;
    if (traceAltInit) {
      int16_t delta = a - traceDernAlt;
      if (delta >= 2) denivelePos += delta;
      else if (delta <= -2) deniveleNeg += -delta;
    }
    traceDernAlt = a; traceAltInit = true;
  }
  float v = gps.speed.isValid() ? gps.speed.kmph() : 0;
  trace[pos].spd = v > 255 ? 255 : (uint8_t)v;
  trace[pos].epoch = (gps.date.isValid() && gps.time.isValid())
      ? versEpoch(gps.date.year(), gps.date.month(), gps.date.day(),
                  gps.time.hour(), gps.time.minute(), gps.time.second())
      : 0;
  journalPoint(trace[pos]);
}

void majTrace(float lat, float lon) {
  if (!sortieActive || tracePause) return;
  float d = tracePremier ? 0 : distanceM(lat, lon, traceDernLat, traceDernLon);

  if (!tracePause && (tracePremier || d >= TRACE_DIST_M || millis() - lastTrace >= TRACE_PERIODE)) {
    if (!tracePremier) distanceTotale += d;
    ajouterPoint(lat, lon);
    traceDernLat = lat; traceDernLon = lon;
    tracePremier = false;
    lastTrace = millis();
  }

  if (gps.speed.isValid() && gps.speed.kmph() > vitesseMax)
    vitesseMax = gps.speed.kmph();

  if (gps.altitude.isValid()) {
    float a = gps.altitude.meters();
    if (!altInit) { altMin = altMax = a; altInit = true; }
    if (a < altMin) altMin = a;
    if (a > altMax) altMax = a;
  }
}

void remettreAZero() {
  traceDebut = traceNb = 0;
  distanceTotale = vitesseMax = 0;
  altInit = false;
  lastTrace = 0;
  traceDernLat = traceDernLon = 0;
  tracePremier = true;
  traceDureeMs = 0; traceDernTick = millis();
  denivelePos = deniveleNeg = 0;
  traceAltInit = false;
  envoyerUBX(0x01,0x10,nullptr,0); // remet aussi l'odometre M8N a zero
}

// ============================================================
#include "sorties.h"
//  Position simulee (pour essayer l'interface sans fix)
// ============================================================

void injecterNMEA(const char* corps) {
  uint8_t ck = 0;
  for (const char* p = corps; *p; p++) ck ^= (uint8_t)*p;
  char trame[130];
  snprintf(trame, sizeof(trame), "$%s*%02X\r\n", corps, ck);
  for (const char* p = trame; *p; p++) gps.encode(*p);
}

void versNMEA(float deg, char* dst, size_t n, bool estLongitude) {
  int d = (int)deg;
  float m = (deg - d) * 60.0f;
  snprintf(dst, n, estLongitude ? "%03d%07.4f" : "%02d%07.4f", d, m);
}

// Parcours fictif : une boucle autour de Cergy-Pontoise.
void simuler() {
  static uint16_t pas = 0;
  float t = pas * 0.06f;
  float lat = 49.0369f + 0.012f * sinf(t);
  float lon = 2.0631f  + 0.020f * cosf(t);
  pas++;

  char slat[16], slon[16], corps[130];
  versNMEA(lat, slat, sizeof(slat), false);
  versNMEA(lon, slon, sizeof(slon), true);

  uint32_t s = (millis() / 1000) % 60;
  snprintf(corps, sizeof(corps),
           "GPGGA,1235%02lu,%s,N,%s,E,1,09,0.9,45.0,M,46.9,M,,",
           (unsigned long)s, slat, slon);
  injecterNMEA(corps);
  snprintf(corps, sizeof(corps),
           "GPRMC,1235%02lu,A,%s,N,%s,E,18.5,054.7,180926,,",
           (unsigned long)s, slat, slon);
  injecterNMEA(corps);
}

// ============================================================
//  Ecran
// ============================================================

void drawCentre(const char* txt, int y) {
  u8g2.drawStr((128 - u8g2.getStrWidth(txt)) / 2, y, txt);
}

// Ecrit un texte centre en choisissant la plus grande police qui tienne.
// "Chennevieres-les-Louvres", le nom le plus long du 95, fait 120 px en 5x8.
void drawTexteAdapte(const char* txt, int y) {
  static const uint8_t* const polices[] = {
    u8g2_font_7x13B_tf, u8g2_font_6x12_tf, u8g2_font_5x8_tf
  };
  for (uint8_t i = 0; i < 3; i++) {
    u8g2.setFont(polices[i]);
    int w = u8g2.getStrWidth(txt);
    if (w <= 128 || i == 2) {
      int x = (128 - w) / 2;
      u8g2.drawStr(x > 0 ? x : 0, y, txt);
      return;
    }
  }
}

void bandeau() {
  char buf[28];
  u8g2.setFont(u8g2_font_5x8_tf);
  snprintf(buf, sizeof(buf), "%u sat", gps.satellites.isValid() ? gps.satellites.value() : 0);
  u8g2.drawStr(0, 7, buf);
  if (gps.time.isValid()) {
    snprintf(buf, sizeof(buf), "%02u:%02u", gps.time.hour(), gps.time.minute());
    u8g2.drawStr(128 - u8g2.getStrWidth(buf), 7, buf);
  }
  u8g2.drawHLine(0, 10, 128);
}

// Derniere page du carrousel : informations de connexion.
void ecranAttente() {
  char buf[48];

  if (modeReseau == MODE_STA_ESSAI) {
    u8g2.setFont(u8g2_font_6x12_tf);
    drawCentre("Connexion au reseau", 14);
    drawTexteAdapte(staSsid.c_str(), 33);
    u8g2.setFont(u8g2_font_7x13B_tf);
    snprintf(buf, sizeof(buf), "%lu s", (unsigned long)decompteSTA());
    drawCentre(buf, 52);
    u8g2.setFont(u8g2_font_5x8_tf);
    drawCentre("4/4 Connexion", 63);
    return;
  }

  u8g2.setFont(u8g2_font_6x12_tf);
  drawCentre(modeReseau == MODE_STA ? staSsid.c_str() : AP_SSID, 16);

  u8g2.setFont(u8g2_font_6x12_tf);
  drawTexteAdapte(urlCourante(), 38);

  u8g2.setFont(u8g2_font_5x8_tf);
  drawCentre(clientPresent() ? "Client connecte" : "Aucun client WiFi", 51);
  drawCentre("4/4 Connexion", 63);
}

void contenu() {
  char buf[40];

  // Carrousel autonome, avec ou sans client WiFi : 5 secondes par page.
  static uint8_t page = 0;
  static uint32_t changement = millis();
  if (millis() - changement >= 5000) { page = (page + 1) % 4; changement = millis(); }
  if (page == 3) { ecranAttente(); return; }
  bandeau();
  u8g2.setFont(u8g2_font_5x8_tf);
  drawCentre(page == 0 ? "1/4 Position" : page == 1 ? "2/4 GPS" : "3/4 Sortie", 63);
  if (page == 2) {
    u8g2.setFont(u8g2_font_6x12_tf);
    drawCentre(sortieActive ? (tracePause ? "Sortie en pause" : "Enregistrement") : "Aucune sortie active", 24);
    snprintf(buf, sizeof(buf), "%.2f km", distanceTotale / 1000.0f);
    drawCentre(buf, 39);
    u8g2.setFont(u8g2_font_5x8_tf);
    uint32_t secondes = traceDureeMs / 1000;
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu max %.1f", (unsigned long)(secondes/3600), (unsigned long)(secondes/60%60), (unsigned long)(secondes%60), vitesseMax);
    drawCentre(buf, 51);
    return;
  }
  if (page == 1 && fixActuel()) {
    u8g2.setFont(u8g2_font_7x13B_tf);
    if (gps.speed.isValid()) snprintf(buf, sizeof(buf), "%.1f km/h", gps.speed.kmph());
    else snprintf(buf, sizeof(buf), "Vitesse --");
    drawCentre(buf, 27);
    u8g2.setFont(u8g2_font_6x12_tf);
    if (gps.altitude.isValid()) snprintf(buf, sizeof(buf), "Altitude %.0f m", gps.altitude.meters());
    else snprintf(buf, sizeof(buf), "Altitude --");
    drawCentre(buf, 41);
    u8g2.setFont(u8g2_font_5x8_tf);
    if (gps.course.isValid()) snprintf(buf, sizeof(buf), "Cap %.0f deg%s", gps.course.deg(), simuActive ? " SIMU" : "");
    else snprintf(buf, sizeof(buf), "Cap --");
    drawCentre(buf, 52);
    return;
  }

  if (!fixActuel()) {
    u8g2.setFont(u8g2_font_6x12_tf);
    drawCentre("Pas de fix", 30);
    u8g2.setFont(u8g2_font_5x8_tf);
    if (gps.charsProcessed() < 10) drawCentre("GPS muet", 46);
    else {
      snprintf(buf, sizeof(buf), "%u sat en vue", satsEnVue);
      drawCentre(buf, 46);
    }
    return;
  }

  drawTexteAdapte(communeNom ? communeNom : "Position GPS", 26);

  u8g2.setFont(u8g2_font_5x8_tf);
  if (!communeNom)
    snprintf(buf, sizeof(buf), "commune 95 a %.0f km", communeDist / 1000.0f);
  else if (communeExacte)
    snprintf(buf, sizeof(buf), "%.1f km du centre", communeDist / 1000.0f);
  else
    snprintf(buf, sizeof(buf), "~ a %.0f m du centre", communeDist);
  drawCentre(buf, 38);

  u8g2.setFont(u8g2_font_5x8_tf);
  snprintf(buf, sizeof(buf), "%.5f %.5f", gps.location.lat(), gps.location.lng());
  drawCentre(buf, 51);
}

// Tampon complet : un transfert par rafraichissement.
void drawScreen() {
  u8g2.clearBuffer();
  contenu();
  u8g2.sendBuffer();
}


// ============================================================
//  Reseau : point d'acces de secours et connexion au routeur
// ============================================================

bool clientPresent() {
  if (millis() - dernierClient < CLIENT_TIMEOUT_MS) return true;
  // En point d'acces, une station associee compte aussi comme presence,
  // meme si elle n'a pas encore ouvert la page.
  return modeReseau == MODE_AP && WiFi.softAPgetStationNum() > 0;
}

const char* urlCourante() {
  static char url[40];
  snprintf(url, sizeof(url), "http://%s/",
           modeReseau == MODE_STA ? staIp.c_str()
                                  : WiFi.softAPIP().toString().c_str());
  return url;
}

void demarrerAP() {
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  bool ok = WiFi.softAP(AP_SSID, AP_PASS, AP_CANAL);
  delay(100);
  modeReseau = MODE_AP;
  staIp = "";
  Serial.printf("Point d'acces \"%s\" %s  ->  %s" "\n",
                AP_SSID, ok ? "actif" : "EN ECHEC", urlCourante());
}

// On garde le point d'acces allume pendant la tentative : le telephone
// reste connecte et voit le decompte au lieu d'etre coupe net.
void tenterSTA(const String& ssid, const String& pass) {
  staSsid = ssid; staPass = pass;
  dernierEchec = "";
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  modeReseau = MODE_STA_ESSAI;
  staDebut = millis();
  Serial.printf("Connexion a \"%s\", %lu s au maximum" "\n",
                ssid.c_str(), (unsigned long)(STA_TIMEOUT_MS / 1000));
}

uint32_t decompteSTA() {
  if (modeReseau != MODE_STA_ESSAI) return 0;
  uint32_t ecoule = millis() - staDebut;
  return ecoule >= STA_TIMEOUT_MS ? 0 : (STA_TIMEOUT_MS - ecoule + 999) / 1000;
}

void surveillerReseau() {
  if (modeReseau == MODE_STA_ESSAI) {
    if (WiFi.status() == WL_CONNECTED) {
      staIp = WiFi.localIP().toString();
      modeReseau = MODE_STA;
      staSucces = millis();
      prefs.putString("ssid", staSsid);
      prefs.putString("pass", staPass);
      Serial.printf("Connecte a \"%s\"  ->  http://%s/" "\n",
                    staSsid.c_str(), staIp.c_str());
      return;
    }
    if (millis() - staDebut >= STA_TIMEOUT_MS) {
      dernierEchec = "delai de 45 s depasse";
      Serial.println("Echec de connexion : retour en point d'acces");
      WiFi.disconnect(true);
      demarrerAP();
    }
    return;
  }

  if (modeReseau == MODE_STA) {
    // Trois secondes de sursis avec le point d'acces encore actif, le temps
    // que la page affiche la nouvelle adresse avant de couper la liaison.
    if (staSucces && millis() - staSucces > 3000) {
      staSucces = 0;
      WiFi.mode(WIFI_STA);
      Serial.println("Point d'acces coupe, la carte est sur le routeur");
    }
    // Perte du routeur : on revient au point d'acces plutot que rester muet.
    if (WiFi.status() != WL_CONNECTED) {
      dernierEchec = "liaison perdue avec le routeur";
      Serial.println("Routeur perdu : retour en point d'acces");
      demarrerAP();
    }
  }
}

// ============================================================
//  Serveur web
// ============================================================

void handleRoot() {
  dernierClient = millis();
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.send_P(200, "text/html; charset=utf-8", PAGE_HTML);
}

// Etat courant. Les noms de communes sont normalises en ASCII sans
// guillemet ni antislash : pas d'echappement JSON necessaire.
void handleApi() {
  dernierClient = millis();
  char json[1200];
  char utc[12] = "--:--:--";
  if (gps.time.isValid())
    snprintf(utc, sizeof(utc), "%02u:%02u:%02u",
             gps.time.hour(), gps.time.minute(), gps.time.second());

  bool fix = fixActuel();
  uint8_t typeFix = ubx.detecte ? ubx.fixType :
      (atoi(gsaFixGn.value()) ? atoi(gsaFixGn.value()) : atoi(gsaFixGps.value()));
  int n = snprintf(json, sizeof(json),
    "{\"fix\":%s,\"ville\":%s%s%s,\"dist\":%.0f,"
    "\"lat\":%.6f,\"lon\":%.6f,\"alt\":%.1f,\"spd\":%.1f,\"cap\":%.0f,"
    "\"sat\":%u,\"vue\":%u,\"hdop\":%.1f,\"utc\":\"%s\",\"nmea\":%lu,"
    "\"dtot\":%.0f,\"vmax\":%.1f,\"altmin\":%.0f,\"altmax\":%.0f,"
    "\"pts\":%u,\"ttff\":%lu,\"up\":%lu,\"simu\":%s,\"clients\":%u,"
    "\"exact\":%s,\"ubx\":%s,\"fixType\":%u,\"hacc\":%.1f,\"vacc\":%.1f,"
    "\"gnssUtil\":%u,\"gpsfw\":\"%s\","
    "\"odo\":%lu,\"odoacc\":%lu,\"profil\":%u,\"pause\":%s,"
    "\"dpos\":%.0f,\"dneg\":%.0f,\"duree\":%lu,\"vmoy\":%.1f,"
    "\"heap\":%lu}",
    fix ? "true" : "false",
    communeNom ? "\"" : "", communeNom ? communeNom : "null", communeNom ? "\"" : "",
    communeDist < 0 ? 0.0f : communeDist,
    fix ? gps.location.lat() : 0.0,
    fix ? gps.location.lng() : 0.0,
    gps.altitude.isValid() ? gps.altitude.meters() : 0.0,
    gps.speed.isValid()    ? gps.speed.kmph()      : 0.0,
    gps.course.isValid()   ? gps.course.deg()      : 0.0,
    gps.satellites.isValid() ? gps.satellites.value() : 0,
    satsEnVue,
    gps.hdop.isValid() ? gps.hdop.hdop() : 99.9,
    utc,
    (unsigned long)gps.charsProcessed(),
    distanceTotale, vitesseMax,
    altInit ? altMin : 0.0f, altInit ? altMax : 0.0f,
    traceNb,
    (unsigned long)(ttff / 1000),
    (unsigned long)(millis() / 1000),
    simuActive ? "true" : "false",
    WiFi.softAPgetStationNum(),
    communeExacte ? "true" : "false",
    ubx.detecte ? "true" : "false", typeFix, ubx.hAcc, ubx.vAcc,
    ubx.nbUtilises, ubx.logiciel,
    (unsigned long)ubx.odoDistance, (unsigned long)ubx.odoPrecision,
    profilM8, tracePause ? "true" : "false",
    denivelePos, deniveleNeg, (unsigned long)(traceDureeMs / 1000),
    traceDureeMs ? distanceTotale * 3600.0f / traceDureeMs : 0.0f,
    (unsigned long)ESP.getFreeHeap());

  // Une troncature silencieuse produirait du JSON invalide et casserait la
  // page sans rien signaler : on le dit sur la console si ca arrive.
  if (n < 0 || n >= (int)sizeof(json))
    Serial.printf("ATTENTION: JSON /api tronque (%d octets pour %u)\n",
                  n, (unsigned)sizeof(json));

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void handleSats() {
  String j = "{\"vue\":" + String(satsEnVue) + ",\"sats\":[";
  bool premier = true;
  for (uint8_t i = 0; i < SAT_MAX; i++) {
    if (!sats[i].actif) continue;
    if (!premier) j += ",";
    premier = false;
    j += "{\"g\":" + String(sats[i].gnss) +
         ",\"prn\":" + String(sats[i].prn) +
         ",\"el\":"  + String(sats[i].elev) +
         ",\"az\":"  + String(sats[i].azim) +
         ",\"snr\":" + String(sats[i].snr) + "}";
  }
  j += "]}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

// Trace envoyee en morceaux : 600 points ne tiennent pas confortablement
// dans une String unique, et le streaming evite de saturer le tas.
void handleTrackJson() {
  server.sendHeader("Cache-Control", "no-store");
  static String bloc;
  bloc.reserve(49152); // Tampon reutilise : une reponse de longueur connue.
  bloc = "{\"n\":" + String(traceNb) + ",\"p\":[";
  for (uint16_t i = 0; i < traceNb; i++) {
    const Point& p = trace[(traceDebut + i) % TRACE_MAX];
    if (i) bloc += ",";
    bloc += "[" + String(p.lat, 6) + "," + String(p.lon, 6) + "," +
            String(p.alt) + "," + String(p.spd) + "]";
  }
  bloc += "]}";
  server.send(200, "application/json", bloc);
}

void handleTrackGpx() {
  server.sendHeader("Content-Disposition", "attachment; filename=trace-valdoise.gpx");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/gpx+xml", "");

  server.sendContent(
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<gpx version=\"1.1\" creator=\"ESP32-S3 GPS Val-d'Oise\" "
    "xmlns=\"http://www.topografix.com/GPX/1/1\">\n<trk><name>Trace Val-d'Oise</name><trkseg>\n");

  String bloc;
  bloc.reserve(1024);
  for (uint16_t i = 0; i < traceNb; i++) {
    const Point& p = trace[(traceDebut + i) % TRACE_MAX];
    bloc += "<trkpt lat=\"" + String(p.lat, 6) + "\" lon=\"" + String(p.lon, 6) + "\">";
    bloc += "<ele>" + String(p.alt) + "</ele>";
    if (p.epoch) {
      time_t t = (time_t)p.epoch;
      struct tm* g = gmtime(&t);
      char iso[32];
      strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%SZ", g);
      bloc += "<time>" + String(iso) + "</time>";
    }
    bloc += "</trkpt>\n";
    if (bloc.length() > 900) { server.sendContent(bloc); bloc = ""; }
  }
  bloc += "</trkseg></trk></gpx>\n";
  server.sendContent(bloc);
  server.sendContent("");
}

// Les 6 communes les plus proches, avec distance et relevement.
void handleProches() {
  if (!fixActuel()) { server.send(200, "application/json", "{\"p\":[]}"); return; }

  float lat = gps.location.lat(), lon = gps.location.lng();
  const float k = cosf(lat * 0.017453292f);

  // Selection des 6 meilleurs par insertion : plus economique qu'un tri complet.
  const uint8_t N = 6;
  float meilleurD[N]; uint16_t meilleurI[N];
  for (uint8_t i = 0; i < N; i++) { meilleurD[i] = 1e12f; meilleurI[i] = 0; }

  for (uint16_t i = 0; i < NB_COMMUNES; i++) {
    float dy = (lat - COMMUNES[i].lat) * 110574.0f;
    float dx = (lon - COMMUNES[i].lon) * 111320.0f * k;
    float d2 = dx * dx + dy * dy;
    if (d2 >= meilleurD[N - 1]) continue;
    uint8_t pos = N - 1;
    while (pos > 0 && meilleurD[pos - 1] > d2) {
      meilleurD[pos] = meilleurD[pos - 1];
      meilleurI[pos] = meilleurI[pos - 1];
      pos--;
    }
    meilleurD[pos] = d2; meilleurI[pos] = i;
  }

  String j = "{\"p\":[";
  for (uint8_t i = 0; i < N; i++) {
    if (meilleurD[i] > 1e11f) break;
    const Commune& c = COMMUNES[meilleurI[i]];
    float d = sqrtf(meilleurD[i]);
    float cap = atan2f((c.lon - lon) * k, c.lat - lat) * 57.29578f;
    if (cap < 0) cap += 360;
    if (i) j += ",";
    j += "{\"nom\":\"" + String(c.nom) + "\",\"d\":" + String(d, 0) +
         ",\"cap\":" + String(cap, 0) + "}";
  }
  j += "]}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

void handleFrance() {
  server.sendHeader("Cache-Control", "max-age=86400");   // contour fige : on le met en cache
  server.send_P(200, "application/json", FRANCE_JSON);
}

void handleReset() {
  if (sortieActive) { server.send(409,"application/json","{\"erreur\":\"Terminez la sortie avant de reinitialiser\"}"); return; }
  remettreAZero();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleSimu() {
  if (sortieActive) { server.send(409,"application/json","{\"erreur\":\"Terminez la sortie avant de changer de source GPS\"}"); return; }
  if (server.hasArg("on")) simuActive = server.arg("on") == "1";
  else                     simuActive = !simuActive;
  if (!simuActive) remettreAZero();
  server.send(200, "application/json",
              String("{\"simu\":") + (simuActive ? "true" : "false") + "}");
}

void handleTracePause() {
  if (!sortieActive) { server.send(409,"application/json","{\"erreur\":\"Aucune sortie en cours\"}"); return; }
  if (stockageErreur) { server.send(507,"application/json","{\"erreur\":\"Stockage indisponible\"}"); return; }
  tracePause = !tracePause;
  tracePremier = true;
  traceAltInit = false;
  if (!tracePause) sortieSegment++;
  if (!checkpoint(2)) { server.send(507,"application/json","{\"erreur\":\"Sauvegarde impossible, sortie en pause\"}"); return; }
  traceDernTick = millis();
  server.send(200, "application/json",
              String("{\"pause\":") + (tracePause ? "true" : "false") + "}");
}

void handleProfil() {
  if (!server.hasArg("p")) { server.send(400,"application/json","{\"erreur\":\"profil manquant\"}"); return; }
  String p = server.arg("p");
  uint8_t v = p == "velo" ? 1 : p == "auto" ? 3 : 0;
  appliquerProfilM8(v);
  prefs.putUChar("profil", v);
  server.send(200,"application/json",String("{\"profil\":") + v + "}");
}

// --- Provisionnement WiFi ---
// Le mot de passe du routeur est saisi par l'utilisateur dans son navigateur
// et conserve en NVS pour que la carte se reconnecte seule au demarrage.
void handleWifi() {
  // Appelee en POST : server.arg() lit le corps du formulaire,
  // le mot de passe ne transite donc pas par l'URL.
  if (!server.hasArg("ssid")) {
    server.send(400, "application/json", "{\"ssid\":\"manquant\"}");
    return;
  }
  String ssid = server.arg("ssid");
  String pass = server.hasArg("pass") ? server.arg("pass") : "";
  if (ssid.length() == 0 || ssid.length() > 32) {
    server.send(400, "application/json", "{\"erreur\":\"SSID invalide\"}");
    return;
  }
  if (pass.length() != 0 && (pass.length() < 8 || pass.length() > 63)) {
    server.send(400, "application/json", "{\"erreur\":\"Mot de passe WiFi invalide\"}");
    return;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", "{\"ok\":true,\"delai\":45}");
  tenterSTA(ssid, pass);          // apres la reponse : la bascule coupe la liaison
}

void handleWifiEtat() {
  const char* m = modeReseau == MODE_AP    ? "ap"
                : modeReseau == MODE_STA   ? "sta" : "essai";
  String j = String("{\"mode\":\"") + m + "\"";
  j += ",\"ap\":\"" AP_SSID "\"";
  String ssidJson;
  ssidJson.reserve(staSsid.length() + 8);
  for (size_t i = 0; i < staSsid.length(); i++) {
    char c = staSsid[i];
    if (c == '\"' || c == '\\') ssidJson += '\\';
    if ((uint8_t)c >= 0x20) ssidJson += c;
  }
  j += ",\"ssid\":\"" + ssidJson + "\"";
  j += ",\"ip\":\"" + String(modeReseau == MODE_STA ? staIp
                          : WiFi.softAPIP().toString()) + "\"";
  j += ",\"decompte\":" + String(decompteSTA());
  j += ",\"echec\":\"" + dernierEchec + "\"";
  j += ",\"client\":" + String(clientPresent() ? "true" : "false");
  j += ",\"rssi\":" + String(modeReseau == MODE_STA ? WiFi.RSSI() : 0);
  j += "}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

void handleWifiOubli() {
  prefs.remove("ssid");
  prefs.remove("pass");
  staSsid = ""; staPass = ""; dernierEchec = "";
  server.send(200, "application/json", "{\"ok\":true}");
  demarrerAP();
}

// Enregistre les routes une seule fois. Le serveur survit aux bascules
// entre point d'acces et routeur : seule l'interface reseau change dessous.
void demarrerServeur() {
  server.on("/",           handleRoot);
  server.on("/sortie/etat", handleSortieEtat);
  server.on("/sortie/debut", HTTP_POST, handleSortieDebut);
  server.on("/sortie/fin", HTTP_POST, handleSortieFin);
  server.on("/historique", handleHistorique);
  server.on("/sortie.gpx", handleArchiveGpx);
  server.on("/sortie/supprimer", HTTP_POST, handleSortieSupprimer);
  server.on("/api",        handleApi);
  server.on("/sats",       handleSats);
  server.on("/track.json", handleTrackJson);
  server.on("/track.gpx",  handleTrackGpx);
  server.on("/proches",    handleProches);
  server.on("/france.json",handleFrance);
  server.on("/reset",      HTTP_POST, handleReset);
  server.on("/simu",       HTTP_POST, handleSimu);
  server.on("/trace/pause",HTTP_POST, handleTracePause);
  server.on("/profil",     HTTP_POST, handleProfil);
  server.on("/wifi",       HTTP_POST, handleWifi);
  server.on("/wifi/etat",  handleWifiEtat);
  server.on("/wifi/oubli", HTTP_POST, handleWifiOubli);
  server.onNotFound(handleRoot);    // tout chemin inconnu renvoie la page
  server.begin();
}

// ============================================================
//  Diagnostic I2C
// ============================================================

void scanI2C() {
  Serial.println("Scan I2C...");
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  peripherique trouve a 0x%02X\n", addr);
      found++;
    }
  }
  if (!found) Serial.println("  aucun peripherique (verifier cablage / pull-ups)");
}

// ============================================================

void setup() {
  Serial.setTxBufferSize(4096);
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0); // Ne jamais attendre qu'un PC lise les diagnostics.
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 2000) { delay(10); }   // CDC : on attend un peu, sans bloquer
  Serial.println("\nEdgeGPS - ESP32-S3 / NEO-M8N / OLED");
  Serial.printf("%u communes du Val-d'Oise (contours) + %lu communes de France\n",
                NB_POLYS, (unsigned long)NB_FR);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  Wire.setTimeOut(20);
  scanI2C();

  u8g2.begin();
  u8g2.setBusClock(400000);

  prefs.begin("edgegps", false);
  initSorties();
  profilM8 = prefs.getUChar("profil", 0);
  if (profilM8 != 1 && profilM8 != 3) profilM8 = 0;
  String memo = prefs.getString("ssid", "");

  // L'ordre compte : server.begin() ouvre une socket, la pile reseau doit
  // donc deja tourner. Demarrer le serveur avant WiFi.mode() fait planter
  // le noyau sur un assert de file d'attente FreeRTOS.
  demarrerAP();                       // le secours reste joignable en toute circonstance
  demarrerServeur();

  if (memo.length()) {
    Serial.printf("Identifiants memorises pour \"%s\", tentative directe" "\n",
                  memo.c_str());
    tenterSTA(memo, prefs.getString("pass", ""));
  }

  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_7x13B_tf);
    drawCentre("EdgeGPS", 24);
    u8g2.setFont(u8g2_font_6x12_tf);
    drawCentre("demarrage", 44);
  } while (u8g2.nextPage());

  Serial1.setRxBufferSize(16384);
  Serial1.setTxBufferSize(2048);
  Serial1.begin(GPS_BAUD_USINE, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  delay(250);
  configurerM8N();
  tDepart = millis();
}

// Commandes depuis la console USB : s = simulation, r = remise a zero,
// i = rappel des identifiants du point d'acces.
void commandesSerie() {
  while (Serial.available()) {
    switch (Serial.read()) {
      case 's': case 'S':
        if (sortieActive) break;
        simuActive = !simuActive;
        if (!simuActive) remettreAZero();
        Serial.printf("simulation %s\n", simuActive ? "ACTIVE" : "coupee");
        break;
      case 'r': case 'R':
        if (sortieActive) break;
        remettreAZero();
        Serial.println("trace et statistiques remises a zero");
        break;
      case 'b': case 'B': {
        // Mesure la recherche nationale sur des points repartis dans toute la France.
        static const float PTS[][2] = {
          {49.0369f, 2.0631f}, {48.8566f, 2.3522f}, {43.2965f, 5.3698f},
          {45.7640f, 4.8357f}, {47.2184f,-1.5536f}, {48.5734f, 7.7521f},
          {44.8378f,-0.5792f}, {42.6976f, 9.4509f}, {50.6292f, 3.0573f},
          {43.6047f, 1.4442f}
        };
        uint32_t tot = 0, exam = 0;
        Serial.println("--- recherche nationale ---");
        for (auto& q : PTS) {
          float d; uint32_t n;
          uint32_t t = micros();
          const CommuneFR* c = plusProcheFR(q[0], q[1], &d, &n);
          t = micros() - t;
          tot += t; exam += n;
          Serial.printf("%8.4f,%8.4f -> %-24s %6.0f m  %5lu us  %lu examinees\n",
                        q[0], q[1], c ? c->nom : "?", d,
                        (unsigned long)t, (unsigned long)n);
        }
        Serial.printf("moyenne : %lu us, %lu entrees examinees sur %lu\n",
                      (unsigned long)(tot / 10), (unsigned long)(exam / 10),
                      (unsigned long)NB_FR);
        break;
      }
      case 'i': case 'I':
        Serial.printf("Sorties: stockage=%s libre=%u active=%u pause=%u id=%u erreur=%u\n", stockageOK?"pret":"indisponible", stockageOK?(unsigned)(LittleFS.totalBytes()-LittleFS.usedBytes()):0,sortieActive,tracePause,(unsigned)sortieId,stockageErreur);
        Serial.printf("mode %s   reseau %s   %s   client %s\n",
                      modeReseau == MODE_AP ? "point d'acces"
                        : modeReseau == MODE_STA ? "routeur" : "connexion en cours",
                      modeReseau == MODE_STA ? staSsid.c_str() : AP_SSID,
                      urlCourante(),
                      clientPresent() ? "present" : "absent");
        break;
      case 't': case 'T':
        // Auto-test : reseau volontairement inexistant, pour verifier que le
        // decompte de 45 s aboutit bien au retour en point d'acces.
        Serial.println("auto-test : tentative sur un reseau inexistant");
        tenterSTA("EdgeGPS-reseau-absent", "motdepassebidon");
        break;
      case 'o': case 'O':
        prefs.remove("ssid"); prefs.remove("pass");
        staSsid = ""; staPass = ""; dernierEchec = "";
        Serial.println("identifiants routeur effaces, retour en point d'acces");
        demarrerAP();
        break;
    }
  }
}

void loop() {
  server.handleClient();
  commandesSerie();
  surveillerReseau();
  surveillerUBX();
  uint32_t maintenant = millis();
  if (sortieActive && !tracePause && traceDernTick) traceDureeMs += maintenant - traceDernTick;
  traceDernTick = maintenant;
  serviceSorties();

  static uint32_t dernierSimu=0;
  if (simuActive && millis()-dernierSimu>=500) { dernierSimu=millis(); simuler(); }

  uint32_t debutLecture = micros();
  while (Serial1.available() && micros() - debutLecture < 2000) {
    char c = Serial1.read();
    if (!simuActive) gps.encode(c);
    decoderUBX((uint8_t)c);
#if ECHO_NMEA
    Serial.write(c);
#endif
  }

  majSatellites();

  // On relance la recherche des que la position a bouge de facon
  // significative. Comparer les coordonnees plutot que se fier a un drapeau
  // de lecture rend le declenchement independant de la source des trames.
  if (fixActuel()) {
    static float lastLat = 0, lastLon = 0;
    float lat = gps.location.lat(), lon = gps.location.lng();
    if (communeDist < 0.0f ||
        fabsf(lat - lastLat) > 1e-4f || fabsf(lon - lastLon) > 1e-4f) {
      lastLat = lat; lastLon = lon;
      chercherCommune(lat, lon);
    }
    if (ttff == 0) ttff = millis() - tDepart;
    majTrace(lat, lon);
  }

  if (millis() - lastDraw >= 500) {
    lastDraw = millis();
    drawScreen();

    // L'ecran reste a 2 Hz ; les diagnostics ne monopolisent plus la boucle.
    static uint32_t dernierLog = 0;
    if (!Serial || Serial.availableForWrite() < 256 || millis() - dernierLog < 2000) return;
    dernierLog = millis();

    if (fixActuel()) {
      Serial.printf("%.6f, %.6f  alt=%.0fm  sat=%u/%u  -> %s (%.0f m)  trace %u pts, %.0f m\n",
                    gps.location.lat(), gps.location.lng(),
                    gps.altitude.meters(),
                    gps.satellites.isValid() ? gps.satellites.value() : 0, satsEnVue,
                    communeNom ? communeNom : "hors Val-d'Oise", communeDist,
                    traceNb, distanceTotale);
    } else {
      Serial.printf("pas de fix - %lu car. NMEA, %u sat, %lu erreurs, UBX sync=%lu ok=%lu ck=%lu\n",
                    (unsigned long)gps.charsProcessed(), satsEnVue,
                    (unsigned long)gps.failedChecksum(), (unsigned long)ubxSync,
                    (unsigned long)ubxTrames, (unsigned long)ubxCkErreur);
    }

    // Le module emet par rafales d'une seconde : comparer deux echantillons
    // espaces de 500 ms donnait de faux "UART muet". On n'alerte donc que
    // si plus rien n'arrive pendant 3 secondes pleines.
    static uint32_t dernierCar = 0;
    if (gps.charsProcessed() != charsAtLastCheck) {
      charsAtLastCheck = gps.charsProcessed();
      dernierCar = millis();
    } else if (!simuActive && millis() - dernierCar > 3000) {
      Serial.println("  -> UART1 muet : verifier TXD GPS sur IO4, GND commun, baud 9600");
      dernierCar = millis();
    }
  }
}
