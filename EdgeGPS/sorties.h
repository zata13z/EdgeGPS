#pragma once
#include <esp_partition.h>
// Journal versionne : chaque enregistrement est autonome et controle.
struct Enregistrement {
  uint32_t magic, duree;
  float distance, maximum, montee, descente;
  Point point;
  uint16_t segment;
  uint8_t type, reserve; // 1 point, 2 checkpoint, 3 sortie terminee
  uint32_t checksum;
};
static bool stockageOK = false, stockageErreur = false;
static Enregistrement attente[64];
static size_t attenteNb = 0;
static uint32_t dernierSauvetage = 0;
static String cheminSortie(uint32_t id) { return "/sortie-" + String(id) + ".bin"; }
static uint32_t somme(const Enregistrement& r) {
  uint32_t h = 2166136261u;
  const uint8_t* p = (const uint8_t*)&r;
  for (size_t i=0; i<offsetof(Enregistrement,checksum); i++) h=(h^p[i])*16777619u;
  return h;
}
static Enregistrement etatJournal(uint8_t type) {
  Enregistrement r = {};
  r.magic=0x45544731; r.duree=traceDureeMs; r.distance=distanceTotale;
  r.maximum=vitesseMax; r.montee=denivelePos; r.descente=deniveleNeg;
  r.segment=sortieSegment; r.type=type; r.reserve=simuActive?1:0; r.checksum=somme(r);
  return r;
}
static bool lireRecord(File& f, Enregistrement& r) {
  return f.read((uint8_t*)&r,sizeof(r))==sizeof(r) && r.magic==0x45544731 && r.checksum==somme(r);
}
bool sauverJournal() {
  if (!stockageOK || !sortieId) return false;
  File f=LittleFS.open(cheminSortie(sortieId),FILE_APPEND);
  bool ok=(bool)f;
  for(size_t i=0; ok && i<attenteNb; i++) ok=f.write((uint8_t*)&attente[i],sizeof(Enregistrement))==sizeof(Enregistrement);
  f.flush(); f.close();
  if(ok) { attenteNb=0; dernierSauvetage=millis(); }
  else { stockageErreur=true; tracePause=true; }
  return ok;
}
void journalPoint(const Point& p) {
  if(!sortieActive || stockageErreur) return;
  if(LittleFS.totalBytes()-LittleFS.usedBytes()<32768) { sauverJournal(); stockageErreur=true; tracePause=true; return; }
  if(attenteNb==64 && !sauverJournal()) return;
  Enregistrement r=etatJournal(1); r.point=p; r.checksum=somme(r);
  attente[attenteNb++]=r;
}
static bool checkpoint(uint8_t type) {
  if(attenteNb==64 && !sauverJournal()) return false;
  attente[attenteNb++]=etatJournal(type);
  return sauverJournal();
}
void initSorties() {
  stockageOK=LittleFS.begin(false);
  if(!stockageOK) {
    // Ne formater qu'une partition integralement vierge, jamais des donnees existantes.
    const esp_partition_t* p=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_ANY,"spiffs");
    bool vierge=p!=nullptr; uint8_t b[1024];
    for(size_t off=0; vierge && off<p->size; off+=sizeof(b)) {
      if(esp_partition_read(p,off,b,sizeof(b))!=ESP_OK) { vierge=false; break; }
      for(uint8_t v:b) if(v!=255) { vierge=false; break; }
    }
    if(vierge && LittleFS.format()) stockageOK=LittleFS.begin(false);
  }
  Serial.printf("Stockage sorties : %s\n",stockageOK?"pret":"indisponible");
  if(!stockageOK) return;
  sortieId=prefs.getUInt("activeTrip",0);
  if(!sortieId) return;
  File f=LittleFS.open(cheminSortie(sortieId),FILE_READ);
  Enregistrement r={}, dernier={}; size_t valide=0;
  while(lireRecord(f,r)) {
    dernier=r; valide+=sizeof(r);
    if(r.type==1) {
      if(!altInit) { altMin=altMax=r.point.alt; altInit=true; }
      altMin=min(altMin,(float)r.point.alt); altMax=max(altMax,(float)r.point.alt);
      uint16_t pos=(traceDebut+traceNb)%TRACE_MAX;
      if(traceNb==TRACE_MAX) { pos=traceDebut; traceDebut=(traceDebut+1)%TRACE_MAX; } else traceNb++;
      trace[pos]=r.point;
    }
  }
  size_t taille=f.size(); f.close();
  if(!valide || valide!=taille) { stockageErreur=true; sortieId=0; return; }
  if(dernier.type==3) { prefs.remove("activeTrip"); sortieId=0; return; }
  traceDureeMs=dernier.duree; distanceTotale=dernier.distance; vitesseMax=dernier.maximum;
  denivelePos=dernier.montee; deniveleNeg=dernier.descente; sortieSegment=dernier.segment;
  sortieActive=true; tracePause=true; tracePremier=true; traceAltInit=false;
  simuActive=dernier.reserve==1;
}
void serviceSorties() {
  if(sortieActive && !tracePause && !stockageErreur && millis()-dernierSauvetage>=10000) checkpoint(2);
}
void handleSortieEtat() {
  String j="{\"active\":"+String(sortieActive?"true":"false")+",\"pause\":"+String(tracePause?"true":"false");
  j+=",\"id\":"+String(sortieId)+",\"stockage\":"+String(stockageOK?"true":"false")+",\"erreur\":"+String(stockageErreur?"true":"false");
  j+=",\"libre\":"+String(stockageOK?LittleFS.totalBytes()-LittleFS.usedBytes():0)+"}";
  server.send(200,"application/json",j);
}
void handleSortieDebut() {
  if(sortieActive || !stockageOK || stockageErreur) { server.send(409,"application/json","{\"erreur\":\"Sortie active ou stockage indisponible\"}"); return; }
  if(LittleFS.totalBytes()-LittleFS.usedBytes()<32768) { server.send(507,"application/json","{\"erreur\":\"Espace insuffisant pour une nouvelle sortie\"}"); return; }
  uint32_t id=prefs.getUInt("tripNext",1);
  while(LittleFS.exists(cheminSortie(id))) id++;
  remettreAZero(); sortieId=id; sortieSegment=0; tracePause=false;
  if(!checkpoint(2)) { sortieId=0; server.send(507,"application/json","{\"erreur\":\"Sauvegarde impossible\"}"); return; }
  prefs.putUInt("tripNext",id+1); prefs.putUInt("activeTrip",id);
  sortieActive=true; traceDernTick=millis(); handleSortieEtat();
}
void handleSortieFin() {
  if(!sortieActive) { server.send(409,"application/json","{\"erreur\":\"Aucune sortie active\"}"); return; }
  tracePause=true;
  if(!checkpoint(3)) { server.send(507,"application/json","{\"erreur\":\"Sauvegarde impossible\"}"); return; }
  sortieActive=false; stockageErreur=false; prefs.remove("activeTrip"); handleSortieEtat();
}
void handleHistorique() {
  String j="{\"sorties\":["; bool first=true;
  File root=LittleFS.open("/"); File f=root.openNextFile();
  while(f) {
    String nom=f.name(); if(nom.startsWith("/")) nom.remove(0,1);
    if(nom.startsWith("sortie-") && nom.endsWith(".bin") && f.size()>=sizeof(Enregistrement)) {
      uint32_t id=nom.substring(7).toInt(); Enregistrement r;
      f.seek(f.size()-sizeof(r));
      if(lireRecord(f,r)) {
        if(!first) j+=","; first=false;
        j+="{\"id\":"+String(id)+",\"distance\":"+String(r.distance,0)+",\"duree\":"+String(r.duree/1000)+",\"simulation\":"+String(r.reserve==1?"true":"false")+",\"terminee\":"+String(r.type==3?"true":"false")+"}";
      }
    }
    f.close(); f=root.openNextFile();
  }
  root.close(); j+="]}"; server.send(200,"application/json",j);
}
void handleArchiveGpx() {
  String arg=server.arg("id");
  for(char c:arg) if(c<'0'||c>'9') { server.send(400,"text/plain","Identifiant invalide"); return; }
  uint32_t id=arg.toInt(); File f=LittleFS.open(cheminSortie(id),FILE_READ);
  if(!id || !f) { server.send(404,"text/plain","Sortie introuvable"); return; }
  server.sendHeader("Content-Disposition","attachment; filename=sortie-"+String(id)+".gpx");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN); server.send(200,"application/gpx+xml","");
  server.sendContent("<?xml version=\"1.0\" encoding=\"UTF-8\"?><gpx version=\"1.1\" creator=\"EdgeGPS\" xmlns=\"http://www.topografix.com/GPX/1/1\"><trk><name>Sortie "+String(id)+"</name>");
  Enregistrement r; bool segment=false; uint16_t precedent=0; String bloc; bloc.reserve(8192);
  while(lireRecord(f,r)) {
    if(r.type!=1) continue;
    if(!segment || precedent!=r.segment) { if(segment) bloc+="</trkseg>"; bloc+="<trkseg>"; segment=true; precedent=r.segment; }
    bloc+="<trkpt lat=\""+String(r.point.lat,6)+"\" lon=\""+String(r.point.lon,6)+"\"><ele>"+String(r.point.alt)+"</ele>";
    if(r.point.epoch) { time_t t=r.point.epoch; struct tm g; gmtime_r(&t,&g); char iso[32]; strftime(iso,sizeof(iso),"%Y-%m-%dT%H:%M:%SZ",&g); bloc+="<time>"+String(iso)+"</time>"; }
    bloc+="</trkpt>";
    if(bloc.length()>7000) { server.sendContent(bloc); bloc=""; }
  }
  f.close(); if(segment) bloc+="</trkseg>"; bloc+="</trk></gpx>"; server.sendContent(bloc); server.sendContent("");
}
void handleSortieSupprimer() {
  uint32_t id=server.arg("id").toInt();
  if(!id || (sortieActive && id==sortieId)) { server.send(409,"application/json","{\"erreur\":\"Impossible de supprimer la sortie active\"}"); return; }
  if(!LittleFS.remove(cheminSortie(id))) { server.send(404,"application/json","{\"erreur\":\"Sortie introuvable ou suppression impossible\"}"); return; }
  server.send(200,"application/json","{\"ok\":true}");
}
