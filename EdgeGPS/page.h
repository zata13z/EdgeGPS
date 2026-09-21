// Page web servie par le point d'acces. Stockee en flash (PROGMEM).
// Aucune ressource externe : l'AP n'a pas d'acces Internet.
#pragma once

static const char PAGE_HTML[] PROGMEM = R"PAGE(<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>GPS France</title>
<style>
  :root{
    --bg:#0d1117; --card:#161b22; --line:#272d38; --line2:#1e242d;
    --txt:#e6edf3; --dim:#8b949e; --ok:#3fb950; --warn:#d29922; --ko:#f85149;
    --acc:#58a6ff; --acc2:#bc8cff;
  }
  @media (prefers-color-scheme: light){
    :root{ --bg:#f6f8fa; --card:#fff; --line:#d8dee4; --line2:#eaeef2;
           --txt:#1f2328; --dim:#636c76; --acc:#0969da; --acc2:#8250df; }
  }
  *{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
  body{margin:0;padding:14px;background:var(--bg);color:var(--txt);
       font:15px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
  .wrap{max-width:680px;margin:0 auto}

  header{display:flex;align-items:center;gap:10px;margin-bottom:12px}
  header h1{font-size:14px;font-weight:600;color:var(--dim);margin:0;
            letter-spacing:.05em;text-transform:uppercase;flex:1}
  .pill{display:inline-block;padding:4px 11px;border-radius:999px;
        font-size:12px;font-weight:600;white-space:nowrap}
  .pill.ok{background:rgba(63,185,80,.16);color:var(--ok)}
  .pill.warn{background:rgba(210,153,34,.16);color:var(--warn)}
  .pill.ko{background:rgba(248,81,73,.16);color:var(--ko)}

  nav{display:flex;gap:4px;background:var(--card);border:1px solid var(--line);
      border-radius:10px;padding:4px;margin-bottom:12px;overflow-x:auto;
      position:sticky;top:6px;z-index:10;box-shadow:0 5px 18px rgba(0,0,0,.18)}
  nav button{flex:1 0 auto;min-width:72px;min-height:42px;border:0;background:transparent;color:var(--dim);
             font:600 13px/1 inherit;padding:9px 8px;border-radius:7px;cursor:pointer;touch-action:manipulation}
  nav button.on{background:var(--acc);color:#fff}
  nav button:focus-visible,.btn:focus-visible{outline:2px solid var(--acc2);outline-offset:2px}

  .card{background:var(--card);border:1px solid var(--line);
        border-radius:12px;padding:16px;margin-bottom:12px}
  .ville{font-size:clamp(24px,7vw,34px);font-weight:700;line-height:1.1;margin:0}
  .sub{color:var(--dim);font-size:13px;margin-top:5px}

  .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(128px,1fr));gap:14px}
  .k{color:var(--dim);font-size:11px;text-transform:uppercase;letter-spacing:.05em}
  .v{font-size:19px;font-weight:600;font-variant-numeric:tabular-nums;margin-top:3px}
  .v small{font-size:12px;font-weight:500;color:var(--dim)}

  table{width:100%;border-collapse:collapse;font-size:14px}
  th{text-align:left;color:var(--dim);font-size:11px;text-transform:uppercase;
     letter-spacing:.05em;font-weight:600;padding:0 0 7px}
  td{padding:7px 0;border-top:1px solid var(--line2);font-variant-numeric:tabular-nums}
  td:last-child,th:last-child{text-align:right}

  canvas{display:block;width:100%;height:auto;border-radius:8px}
  .btns{display:flex;gap:8px;flex-wrap:wrap}
  .btn{flex:1;min-width:120px;min-height:44px;text-align:center;text-decoration:none;
        border:1px solid var(--line);background:var(--card);color:var(--txt);
        border-radius:9px;padding:11px 12px;font:600 13px/1 inherit;cursor:pointer;touch-action:manipulation}
  .btn:disabled{opacity:.55;cursor:wait}
  .btn.p{background:var(--acc);border-color:var(--acc);color:#fff}
  .btn.on{background:var(--acc2);border-color:var(--acc2);color:#fff}
  .btn.d{color:var(--ko);border-color:rgba(248,81,73,.4)}
  .inp{width:100%;margin-top:5px;padding:11px 12px;border-radius:9px;
       border:1px solid var(--line);background:var(--bg);color:var(--txt);
       font:15px/1 inherit}
  .inp:focus{outline:2px solid var(--acc);outline-offset:-1px}
  .hide{display:none}
  footer{color:var(--dim);font-size:11px;text-align:center;margin-top:16px;line-height:1.7}
</style>
</head>
<body>
<div class="wrap">

  <header>
    <h1>GPS France &middot; 34 969 communes</h1>
    <span id="etat" class="pill ko" role="status" aria-live="polite">connexion</span>
  </header>

  <nav>
    <button class="on" data-t="sortie">Sortie</button>
    <button data-t="hist">Historique</button>
    <button data-t="pos">Position</button>
    <button data-t="fra">Carte</button>
    <button data-t="sat">Ciel</button>
    <button data-t="trk">Trace</button>
    <button data-t="sta">Stats</button>
    <button data-t="net">Reseau</button>
  </nav>

  <!-- ---------------- POSITION ---------------- -->
  <section id="sortie">
    <div class="card">
      <div class="k" id="connexion">Connexion à la carte…</div>
      <h2 id="sortieTitre">Prêt pour une sortie</h2>
      <div class="sub" id="qualite">En attente du GPS</div>
    </div>
    <div class="card grid">
      <div><div class="k">Vitesse · km/h</div><div class="ville" id="sortieVitesse">--</div></div>
      <div><div class="k">Distance · km</div><div class="ville" id="sortieDistance">0.00</div></div>
      <div><div class="k">Durée hors pause</div><div class="v" id="sortieDuree">0s</div></div>
      <div><div class="k">Moyenne · km/h</div><div class="v" id="sortieMoyenne">0.0</div></div>
    </div>
    <div class="card">
      <div class="btns">
        <button class="btn p" id="depart" disabled>Démarrer</button>
        <button class="btn" id="sortiePause" disabled>Pause</button>
        <button class="btn d" id="terminer" disabled>Terminer</button>
      </div>
      <p id="sortieMessage" role="status" aria-live="polite"></p>
      <div class="sub" id="stockage"></div>
      <div class="sub">Sauvegarde toutes les 10 secondes. Après une coupure, la sortie revient en pause : appuyez sur Reprendre. Sans position GPS, aucun point n'est enregistré.</div>
    </div>
  </section>
  <section id="hist" class="hide">
    <div class="card"><h2>Mes sorties</h2><p class="sub">Parcours sauvegardés dans la carte. Le GPX contient les points enregistrés, au-delà des 600 points de l'aperçu.</p><div id="listeSorties">Chargement…</div></div>
  </section>
  <section id="pos" class="hide">
    <div class="card">
      <p class="ville" id="ville">--</p>
      <div class="sub" id="dist"></div>
    </div>

    <div class="card grid">
      <div><div class="k">Latitude</div><div class="v" id="lat">--</div></div>
      <div><div class="k">Longitude</div><div class="v" id="lon">--</div></div>
      <div><div class="k">Altitude</div><div class="v" id="alt">--</div></div>
      <div><div class="k">Vitesse</div><div class="v" id="spd">--</div></div>
      <div><div class="k">Cap</div><div class="v" id="cap">--</div></div>
      <div><div class="k">Heure UTC</div><div class="v" id="utc">--</div></div>
      <div><div class="k">Precision horizontale</div><div class="v" id="hacc">--</div></div>
      <div><div class="k">Precision verticale</div><div class="v" id="vacc">--</div></div>
    </div>

    <div class="card">
      <table>
        <thead><tr><th>Communes proches</th><th>Direction</th><th>Distance</th></tr></thead>
        <tbody id="proches"><tr><td colspan="3" style="color:var(--dim)">en attente d'un fix</td></tr></tbody>
      </table>
    </div>

    <div class="card">
      <div class="btns">
        <a class="btn" id="osm" href="#" target="_blank" rel="noopener">Voir sur OpenStreetMap</a>
      </div>
      <div class="sub" style="margin-top:9px">Necessite une connexion Internet, donc un autre reseau que celui de la carte.</div>
    </div>
  </section>


  <!-- ---------------- CARTE DE FRANCE ---------------- -->
  <section id="fra" class="hide">
    <div class="card">
      <div class="k" style="margin-bottom:10px">Position en France</div>
      <canvas id="fr" width="620" height="620"></canvas>
      <div class="sub" id="frinfo">En attente d'une position.</div>
    </div>
  </section>

  <!-- ---------------- SATELLITES ---------------- -->
  <section id="sat" class="hide">
    <div class="card">
      <div class="k" style="margin-bottom:10px">Vue du ciel &middot; <span id="nsat">0</span> satellites</div>
      <canvas id="sky" width="640" height="640"></canvas>
      <div class="sub">Le centre est le zenith, le bord l'horizon. Nord en haut.
        La taille et la couleur du disque donnent le rapport signal/bruit.</div>
    </div>
    <div class="card">
      <div class="k" style="margin-bottom:10px">Rapport signal/bruit (dB)</div>
      <canvas id="bars" width="640" height="260"></canvas>
    </div>
  </section>

  <!-- ---------------- TRACE ---------------- -->
  <section id="trk" class="hide">
    <div class="card">
      <div class="k" style="margin-bottom:10px">Trace &middot; <span id="npts">0</span> points</div>
      <canvas id="map" width="640" height="500"></canvas>
      <div class="sub" id="mapinfo">Aucun point enregistre.</div>
    </div>
    <div class="card">
      <div class="btns">
        <a class="btn p" href="/track.gpx">Telecharger le GPX</a>
        <button class="btn" id="pause">Mettre en pause</button>
        <button class="btn d" id="raz">Effacer la trace</button>
      </div>
    </div>
  </section>

  <!-- ---------------- STATS ---------------- -->
  <section id="sta" class="hide">
    <div class="card grid">
      <div><div class="k">Distance</div><div class="v" id="dtot">--</div></div>
      <div><div class="k">Vitesse max</div><div class="v" id="vmax">--</div></div>
      <div><div class="k">Altitude min</div><div class="v" id="amin">--</div></div>
      <div><div class="k">Altitude max</div><div class="v" id="amax">--</div></div>
      <div><div class="k">Points de trace</div><div class="v" id="pts">--</div></div>
      <div><div class="k">Temps 1er fix</div><div class="v" id="ttff">--</div></div>
    </div>
    <div class="card grid">
      <div><div class="k">Satellites utilises</div><div class="v" id="satu">--</div></div>
      <div><div class="k">HDOP</div><div class="v" id="hdop">--</div></div>
      <div><div class="k">Trames NMEA</div><div class="v" id="nmea">--</div></div>
      <div><div class="k">Clients WiFi</div><div class="v" id="cli">--</div></div>
      <div><div class="k">Memoire libre</div><div class="v" id="heap">--</div></div>
      <div><div class="k">Fonctionnement</div><div class="v" id="up">--</div></div>
      <div><div class="k">Recepteur GNSS</div><div class="v" id="gpsmode">--</div></div>
      <div><div class="k">Type de fix</div><div class="v" id="fixtype">--</div></div>
      <div><div class="k">Vitesse moyenne</div><div class="v" id="vmoy">--</div></div>
      <div><div class="k">Duree active</div><div class="v" id="duree">--</div></div>
      <div><div class="k">Denivele + / -</div><div class="v" id="deniv">--</div></div>
      <div><div class="k">Odometre M8N</div><div class="v" id="odo">--</div></div>
    </div>
    <div class="card">
      <div class="k" style="margin-bottom:10px">Profil de navigation M8N</div>
      <div class="btns" id="profils">
        <button class="btn" data-p="pieton">Pieton / Course</button>
        <button class="btn" data-p="velo">Velo</button>
        <button class="btn" data-p="auto">Automobile</button>
      </div>
      <div class="sub" style="margin:9px 0 16px">Adapte les filtres internes du recepteur et son odometre au type de mouvement.</div>
      <div class="btns">
        <button class="btn" id="simu">Position simulee</button>
      </div>
      <div class="sub" style="margin-top:9px">Injecte un parcours fictif autour de Cergy pour essayer
        l'interface sans fix. A couper pour revenir au GPS reel.</div>
    </div>
  </section>


  <!-- ---------------- RESEAU ---------------- -->
  <section id="net" class="hide">
    <div class="card">
      <div class="k">Mode actuel</div>
      <div class="v" id="nmode" style="font-size:22px">--</div>
      <div class="sub" id="nsub"></div>
    </div>

    <div class="card">
      <div class="k" style="margin-bottom:12px">Connecter la carte a votre box</div>
      <label class="k" for="wssid">Nom du reseau</label>
      <input id="wssid" class="inp" autocomplete="off" autocapitalize="none" spellcheck="false" placeholder="SSID de votre box">
      <label class="k" for="wpass" style="margin-top:10px;display:block">Mot de passe</label>
      <input id="wpass" class="inp" type="password" autocomplete="off" placeholder="cle WPA2">
      <div class="btns" style="margin-top:14px">
        <button class="btn p" id="wgo">Connecter</button>
        <button class="btn d" id="wforget">Oublier le reseau</button>
      </div>
      <div class="sub" id="wmsg" style="margin-top:10px">
        La carte tentera la connexion pendant 45 secondes. En cas d'echec elle
        revient d'elle-meme sur le point d'acces EdgeGPS.
      </div>
    </div>

    <div class="card">
      <div class="sub">
        Le mot de passe est transmis en clair sur le reseau local de la carte
        et conserve dans sa memoire non volatile. N'utilisez cette page que
        sur un reseau de confiance, et « Oublier le reseau » avant de preter
        ou de revendre le module.
      </div>
    </div>
  </section>

  <footer>
    ZataLabs &middot; Communes : API Geo (IGN/INSEE) &middot; ESP32-S3 + u-blox M8N<br>
    derniere mise a jour <span id="age">--</span>
  </footer>
</div>

<script>
const $ = id => document.getElementById(id);
async function requete(u, options = {}) {
  const controle = new AbortController();
  const delai = setTimeout(() => controle.abort(), 5000);
  try {
    const r = await fetch(u, {...options, cache:'no-store', signal:controle.signal});
    if (!r.ok) { const erreur = await r.json().catch(() => ({})); throw new Error(erreur.erreur || 'HTTP '+r.status); }
    return await r.json();
  } finally { clearTimeout(delai); }
}
let onglet = 'sortie', dernier = null;
let sortieEtat = null, actionSortieOccupee = false, historiqueActualise = 0;
function afficherSortie(s) {
  sortieEtat=s;
  $('sortieTitre').textContent=s.active ? 'Sortie '+s.id+(s.pause?' · en pause':' · enregistrement') : 'Prêt pour une sortie';
  $('depart').disabled=actionSortieOccupee || s.active || !s.stockage || s.erreur;
  $('sortiePause').disabled=actionSortieOccupee || !s.active || s.erreur;
  $('terminer').disabled=actionSortieOccupee || !s.active;
  $('sortiePause').textContent=s.pause?'Reprendre':'Pause';
  $('stockage').textContent=s.erreur?'Erreur de sauvegarde : enregistrement suspendu.':s.stockage?Math.floor(s.libre/1024)+' Ko disponibles pour les sorties':'Stockage indisponible';
}
async function actionSortie(url) {
  if(actionSortieOccupee) return;
  actionSortieOccupee=true;
  if(sortieEtat) afficherSortie(sortieEtat);
  $('sortieMessage').textContent='En cours…';
  try {
    await requete(url,{method:'POST'});
    afficherSortie(await requete('/sortie/etat'));
    $('sortieMessage').textContent='Enregistré.';
    tick();
  } catch(e) { $('sortieMessage').textContent=e.message+' — vérifiez l’état avant de réessayer.'; }
  finally { actionSortieOccupee=false; if(sortieEtat) afficherSortie(sortieEtat); }
}
function afficherHistorique(d) {
  const liste=$('listeSorties'); liste.replaceChildren();
  if(!d.sorties.length) { liste.textContent='Aucune sortie enregistrée. Démarrez votre première sortie.'; return; }
  d.sorties.sort((a,b)=>b.id-a.id).forEach(s=>{
    const p=document.createElement('p'), a=document.createElement('a');
    p.textContent='Sortie '+s.id+(s.simulation?' · SIMULATION':'')+' · '+(s.distance/1000).toFixed(2)+' km · '+fmtDuree(s.duree)+(s.terminee?' · terminée':' · en cours')+' ';
    a.className='btn'; a.href='/sortie.gpx?id='+s.id; a.textContent='GPX'; p.append(a); liste.append(p);
    if(s.terminee) {
      const b=document.createElement('button'); b.className='btn d'; b.textContent='Supprimer';
      b.onclick=async()=>{
        if(!confirm('Supprimer définitivement la sortie '+s.id+' ? Téléchargez le GPX avant de continuer.')) return;
        b.disabled=true;
        try { await requete('/sortie/supprimer?id='+s.id,{method:'POST'}); afficherHistorique(await requete('/historique')); }
        catch(e) { b.disabled=false; alert(e.message); }
      }; p.append(b);
    }
  });
}
const get = (u,f) => requete(u).then(f).catch(() => signaler('carte injoignable'));
const post = (u,f) => requete(u,{method:'POST'}).then(f).catch(e => signaler(e.message));
function signaler(message){
  $('etat').className='pill ko';
  $('etat').textContent=message;
}

// ------- onglets -------
document.querySelectorAll('nav button').forEach(b => {
  b.onclick = () => {
    document.querySelectorAll('nav button').forEach(x => x.classList.toggle('on', x === b));
    ['sortie','hist','pos','fra','sat','trk','sta','net'].forEach(s => $(s).classList.toggle('hide', s !== b.dataset.t));
    onglet = b.dataset.t;
    historiqueActualise=0;
    rafraichirOnglet();
  };
});

const css = v => getComputedStyle(document.documentElement).getPropertyValue(v).trim();
const fmtDuree = s => {
  if (!s) return '0s';
  const h = Math.floor(s/3600), m = Math.floor(s%3600/60), r = s%60;
  return h ? h+'h '+String(m).padStart(2,'0') : (m ? m+'m '+String(r).padStart(2,'0')+'s' : r+'s');
};
const rose = c => ['N','NE','E','SE','S','SO','O','NO'][Math.round(c/45)%8];

// ------- etat principal -------
function maj(d){
  dernier = d;
  $('connexion').textContent='Carte connectée · données actualisées';
  $('sortieVitesse').textContent=d.fix?d.spd.toFixed(1):'--';
  $('sortieDistance').textContent=(d.dtot/1000).toFixed(2);
  $('sortieDuree').textContent=fmtDuree(d.duree)||'0s';
  $('sortieMoyenne').textContent=(d.vmoy||0).toFixed(1);
  $('qualite').textContent=d.simu?'SIMULATION — parcours fictif':d.fix?'Position acquise · '+d.sat+' satellites':'Recherche GPS · placez le module avec une vue dégagée du ciel';
  const e = $('etat');
  if (d.fix){
    e.className = 'pill ok';
    e.textContent = d.simu ? 'simulation' : 'fix ' + d.sat + ' sat';
    if (d.simu) e.className = 'pill warn';
    $('ville').textContent = d.ville || "Hors Val-d'Oise";
    const km = m => m < 1000 ? m.toFixed(0)+' m' : (m/1000).toFixed(1)+' km';
    $('dist').textContent = !d.ville
      ? 'commune du 95 la plus proche a ' + (d.dist/1000).toFixed(0) + ' km'
      : d.exact
        ? 'position dans les limites communales, a ' + km(d.dist) + ' du centre-ville'
        : 'hors de tout contour connu, commune la plus proche a ' + km(d.dist);
    $('lat').textContent = d.lat.toFixed(5);
    $('lon').textContent = d.lon.toFixed(5);
    $('alt').innerHTML = d.alt.toFixed(0) + ' <small>m</small>';
    $('spd').innerHTML = d.spd.toFixed(1) + ' <small>km/h</small>';
    $('cap').innerHTML = d.cap.toFixed(0) + '&deg; <small>' + rose(d.cap) + '</small>';
    const o = $('osm');
    o.href = 'https://www.openstreetmap.org/?mlat='+d.lat+'&mlon='+d.lon+'#map=15/'+d.lat+'/'+d.lon;
  } else {
    e.className = 'pill ko';
    e.textContent = d.nmea > 0 ? 'pas de fix' : 'GPS muet';
    $('ville').textContent = '--';
    $('dist').textContent = d.nmea > 0
      ? d.vue + ' satellite(s) en vue, pas encore assez pour se positionner'
      : 'aucune trame recue sur UART1 : verifier le cablage du module';
    ['lat','lon','alt','spd','cap'].forEach(k => $(k).textContent = '--');
  }
  $('utc').textContent  = d.utc;
  $('hacc').innerHTML = d.ubx && d.fix ? d.hacc.toFixed(1)+' <small>m</small>' : '--';
  $('vacc').innerHTML = d.ubx && d.fix ? d.vacc.toFixed(1)+' <small>m</small>' : '--';

  $('dtot').innerHTML = d.dtot < 1000 ? d.dtot.toFixed(0)+' <small>m</small>'
                                      : (d.dtot/1000).toFixed(2)+' <small>km</small>';
  $('vmax').innerHTML = d.vmax.toFixed(1)+' <small>km/h</small>';
  $('amin').innerHTML = d.altmin.toFixed(0)+' <small>m</small>';
  $('amax').innerHTML = d.altmax.toFixed(0)+' <small>m</small>';
  $('pts').textContent  = d.pts;
  $('ttff').textContent = d.ttff ? fmtDuree(d.ttff) : '--';
  $('satu').textContent = d.sat + ' / ' + d.vue;
  $('hdop').textContent = d.fix ? d.hdop.toFixed(1) : '--';
  $('nmea').textContent = d.nmea.toLocaleString('fr-FR');
  $('cli').textContent  = d.clients;
  $('heap').innerHTML   = (d.heap/1024).toFixed(0)+' <small>ko</small>';
  $('up').textContent   = fmtDuree(d.up);
  $('gpsmode').textContent = d.ubx ? 'M8N / UBX 5 Hz' : 'NMEA';
  $('gpsmode').title = d.gpsfw || '';
  $('fixtype').textContent = d.fixType === 3 ? '3D' : d.fixType === 2 ? '2D' : 'aucun';
  $('vmoy').innerHTML = d.vmoy.toFixed(1)+' <small>km/h</small>';
  $('duree').textContent = fmtDuree(d.duree);
  $('deniv').innerHTML = d.dpos.toFixed(0)+' / '+d.dneg.toFixed(0)+' <small>m</small>';
  $('odo').innerHTML = d.ubx ? d.odo+' <small>m &plusmn; '+d.odoacc+' m</small>' : '--';
  $('pause').textContent = d.pause ? 'Reprendre la trace' : 'Mettre en pause';
  $('pause').classList.toggle('on', d.pause);
  const profils = {0:'pieton',1:'velo',3:'auto'};
  document.querySelectorAll('#profils button').forEach(b => b.classList.toggle('on', b.dataset.p === profils[d.profil]));
  $('npts').textContent = d.pts;
  $('nsat').textContent = d.vue;
  $('simu').textContent = d.simu ? 'Couper la simulation' : 'Position simulee';
  $('simu').classList.toggle('p', d.simu);
  $('age').textContent  = new Date().toLocaleTimeString('fr-FR');
}

// ------- vue du ciel -------
function dessinerCiel(sats){
  const c = $('sky'), x = c.getContext('2d');
  const W = c.width, R = W/2 - 30, cx = W/2, cy = W/2;
  x.clearRect(0,0,W,W);
  x.strokeStyle = css('--line'); x.fillStyle = css('--dim');
  x.lineWidth = 2; x.font = '600 20px system-ui'; x.textAlign = 'center';

  [0,30,60].forEach(el => {
    x.beginPath(); x.arc(cx,cy,R*(90-el)/90,0,7); x.stroke();
  });
  x.beginPath(); x.moveTo(cx-R,cy); x.lineTo(cx+R,cy);
  x.moveTo(cx,cy-R); x.lineTo(cx,cy+R); x.stroke();
  ['N','E','S','O'].forEach((l,i) => {
    const a = i*Math.PI/2;
    x.fillText(l, cx + Math.sin(a)*(R+18), cy - Math.cos(a)*(R+18) + 7);
  });

  sats.forEach(s => {
    const r = R * (90 - Math.min(s.el,90)) / 90;
    const a = s.az * Math.PI/180;
    const px = cx + Math.sin(a)*r, py = cy - Math.cos(a)*r;
    const bon = s.snr >= 35, moyen = s.snr >= 20;
    x.beginPath(); x.arc(px,py, s.snr ? 13 + s.snr/6 : 9, 0, 7);
    x.fillStyle = s.snr === 0 ? 'transparent' : (bon ? css('--ok') : moyen ? css('--warn') : css('--ko'));
    x.fill();
    x.strokeStyle = s.snr === 0 ? css('--dim') : 'transparent';
    x.lineWidth = 2; x.stroke();
    x.fillStyle = s.snr ? '#fff' : css('--dim');
    x.font = '600 15px system-ui';
    const prefixe = ['G','S','E','B','I','Q','R'][s.g] || '?';
    x.fillText(prefixe+s.prn, px, py+5);
  });

  if (!sats.length){
    x.fillStyle = css('--dim'); x.font = '600 24px system-ui';
    x.fillText('aucun satellite en vue', cx, cy);
  }
}

// ------- barres de SNR -------
function dessinerBarres(sats){
  const c = $('bars'), x = c.getContext('2d');
  const W = c.width, H = c.height;
  x.clearRect(0,0,W,H);
  if (!sats.length){
    x.fillStyle = css('--dim'); x.font='600 20px system-ui'; x.textAlign='center';
    x.fillText('aucune donnee', W/2, H/2); return;
  }
  const n = sats.length, pas = W/n, lw = Math.min(pas*0.66, 46);
  const base = H - 34, hmax = base - 14;
  x.textAlign = 'center';
  sats.forEach((s,i) => {
    const cx = pas*(i+0.5);
    const h = Math.max(2, Math.min(s.snr,55)/55*hmax);
    x.fillStyle = s.snr === 0 ? css('--line')
                : s.snr >= 35 ? css('--ok') : s.snr >= 20 ? css('--warn') : css('--ko');
    x.fillRect(cx-lw/2, base-h, lw, h);
    x.fillStyle = css('--dim'); x.font = '600 16px system-ui';
    const prefixe = ['G','S','E','B','I','Q','R'][s.g] || '?';
    x.fillText(prefixe+s.prn, cx, H-14);
    if (s.snr){ x.fillStyle = css('--txt'); x.fillText(s.snr, cx, base-h-6); }
  });
  x.strokeStyle = css('--line'); x.lineWidth = 2;
  x.beginPath(); x.moveTo(0,base); x.lineTo(W,base); x.stroke();
}

// ------- trace -------
function dessinerTrace(t){
  const c = $('map'), x = c.getContext('2d');
  const W = c.width, H = c.height, m = 26;
  x.clearRect(0,0,W,H);
  if (!t.p || t.p.length < 2){
    x.fillStyle = css('--dim'); x.font='600 20px system-ui'; x.textAlign='center';
    x.fillText('trace trop courte', W/2, H/2);
    $('mapinfo').textContent = 'Il faut au moins deux points pour tracer un parcours.';
    return;
  }
  const lats = t.p.map(p=>p[0]), lons = t.p.map(p=>p[1]);
  const la0 = Math.min(...lats), la1 = Math.max(...lats);
  const lo0 = Math.min(...lons), lo1 = Math.max(...lons);
  const k = Math.cos((la0+la1)/2 * Math.PI/180);
  // Echelle isotrope : on ne deforme pas le parcours.
  const dx = Math.max((lo1-lo0)*k, 1e-6), dy = Math.max(la1-la0, 1e-6);
  const s = Math.min((W-2*m)/dx, (H-2*m)/dy);
  const ox = (W - dx*s)/2, oy = (H - dy*s)/2;
  const X = p => ox + (p[1]-lo0)*k*s;
  const Y = p => H - oy - (p[0]-la0)*s;

  x.strokeStyle = css('--acc'); x.lineWidth = 4;
  x.lineJoin = x.lineCap = 'round';
  x.beginPath(); x.moveTo(X(t.p[0]), Y(t.p[0]));
  t.p.forEach(p => x.lineTo(X(p), Y(p)));
  x.stroke();

  x.fillStyle = css('--dim'); x.beginPath();
  x.arc(X(t.p[0]), Y(t.p[0]), 8, 0, 7); x.fill();
  x.fillStyle = css('--ok'); x.beginPath();
  const d = t.p[t.p.length-1];
  x.arc(X(d), Y(d), 10, 0, 7); x.fill();

  const larg = (lo1-lo0)*k*111320, haut = (la1-la0)*110574;
  $('mapinfo').textContent = t.p.length + ' points &middot; emprise '
    + (larg>1000 ? (larg/1000).toFixed(2)+' km' : larg.toFixed(0)+' m') + ' x '
    + (haut>1000 ? (haut/1000).toFixed(2)+' km' : haut.toFixed(0)+' m')
    + ' - depart en gris, position actuelle en vert';
  $('mapinfo').innerHTML = $('mapinfo').textContent;
}


// ------- carte de France -------
let contour = null;
function dessinerFrance(d){
  const c = $('fr'), x = c.getContext('2d'), W = c.width, H = c.height, m = 18;
  x.clearRect(0,0,W,H);
  if (!contour){ 
    x.fillStyle=css('--dim'); x.font='600 20px system-ui'; x.textAlign='center';
    x.fillText('chargement du contour...', W/2, H/2); return;
  }
  const pts = contour.flat();
  const lo0=Math.min(...pts.map(p=>p[0])), lo1=Math.max(...pts.map(p=>p[0]));
  const la0=Math.min(...pts.map(p=>p[1])), la1=Math.max(...pts.map(p=>p[1]));
  // Projection equirectangulaire corrigee : sans le cos(lat) la France est ecrasee.
  const k = Math.cos((la0+la1)/2*Math.PI/180);
  const dx=(lo1-lo0)*k, dy=la1-la0;
  const s = Math.min((W-2*m)/dx, (H-2*m)/dy);
  const ox=(W-dx*s)/2, oy=(H-dy*s)/2;
  const PX = lo => ox + (lo-lo0)*k*s;
  const PY = la => H - oy - (la-la0)*s;

  // Remplissage discret mais lisible, contour marque : sur un fond sombre
  // le gris de separation des cartes ne se distinguait pas assez.
  x.fillStyle = css('--line2'); x.strokeStyle = css('--dim');
  x.lineWidth = 2.5; x.lineJoin = 'round';
  contour.forEach(b => {
    x.beginPath(); x.moveTo(PX(b[0][0]), PY(b[0][1]));
    b.forEach(p => x.lineTo(PX(p[0]), PY(p[1])));
    x.closePath(); x.fill(); x.stroke();
  });

  if (!d || !d.fix){ $('frinfo').textContent = "En attente d'une position."; return; }

  const px = PX(d.lon), py = PY(d.lat);
  // Croix rouge
  x.strokeStyle = '#e5484d'; x.lineWidth = 4; x.lineCap = 'round';
  x.beginPath();
  x.moveTo(px-13, py-13); x.lineTo(px+13, py+13);
  x.moveTo(px+13, py-13); x.lineTo(px-13, py+13);
  x.stroke();
  // Halo pour la reperer sur un fond charge
  x.strokeStyle = 'rgba(229,72,77,.35)'; x.lineWidth = 2;
  x.beginPath(); x.arc(px, py, 22, 0, 7); x.stroke();

  x.fillStyle = css('--txt'); x.font = '600 17px system-ui';
  x.textAlign = px > W*0.7 ? 'right' : 'left';
  x.fillText(d.ville || 'hors France', px + (px > W*0.7 ? -30 : 30), py + 6);

  $('frinfo').textContent = (d.ville || 'position hors France')
    + ' - ' + d.lat.toFixed(4) + ', ' + d.lon.toFixed(4);
}

// ------- communes proches -------
function majProches(d){
  const tb = $('proches');
  if (!d.p || !d.p.length){
    tb.innerHTML = '<tr><td colspan="3" style="color:var(--dim)">en attente d\'un fix</td></tr>';
    return;
  }
  tb.innerHTML = d.p.map(c =>
    '<tr><td>'+c.nom+'</td><td style="color:var(--dim)">'+rose(c.cap)+'</td><td>'
    + (c.d < 1000 ? c.d+' m' : (c.d/1000).toFixed(1)+' km') + '</td></tr>').join('');
}


// ------- reseau -------
function majReseau(n){
  const m = $('nmode'), sub = $('nsub');
  if (n.mode === 'ap'){
    m.textContent = "Point d'acces " + n.ap;
    sub.textContent = 'Page accessible sur http://' + n.ip + '/';
  } else if (n.mode === 'essai'){
    m.textContent = 'Connexion a ' + n.ssid;
    sub.textContent = "plus que " + n.decompte + " s avant retour au point d'acces";
  } else {
    m.textContent = 'Connecte a ' + n.ssid;
    sub.textContent = 'http://' + n.ip + '/  -  signal ' + n.rssi + ' dBm'
      + " - le point d'acces EdgeGPS est coupe";
  }
  if (n.echec && n.mode === 'ap')
    $('wmsg').textContent = "Derniere tentative : " + n.echec + ".";
}

$('wgo').onclick = () => {
  const ssid = $('wssid').value.trim();
  if (!ssid){ $('wmsg').textContent = "Indiquez le nom du reseau."; $('wssid').focus(); return; }
  // POST et non GET : un mot de passe dans une URL finirait dans
  // l'historique du navigateur et dans les journaux.
  const corps = 'ssid=' + encodeURIComponent(ssid)
              + '&pass=' + encodeURIComponent($('wpass').value);
  $('wmsg').textContent = "Tentative en cours, 45 s...";
  $('wgo').disabled = true;
  fetch('/wifi', {method:'POST', cache:'no-store',
                  headers:{'Content-Type':'application/x-www-form-urlencoded'},
                  body: corps}).then(r=>r.json()).then(()=>{
    $('wpass').value = '';
  }).catch(()=>{ $('wmsg').textContent = "La carte n'a pas repondu."; })
    .finally(()=>{ $('wgo').disabled = false; });
};

$('wforget').onclick = () => {
  post('/wifi/oubli', () => { $('wmsg').textContent = "Identifiants effaces."; });
};

// ------- boucles de rafraichissement -------
let ongletOccupe = false;
async function rafraichirOnglet(){
  if (ongletOccupe || document.hidden) return;
  ongletOccupe = true;
  try {
  if (onglet === 'sortie') await get('/sortie/etat', afficherSortie);
  if (onglet === 'hist' && Date.now()-historiqueActualise>10000) await get('/historique', d=>{afficherHistorique(d); historiqueActualise=Date.now();});
  if (onglet === 'fra'){
    if (!contour) await get('/france.json', d => { contour = d; dessinerFrance(dernier); });
    else dessinerFrance(dernier);
  }
  if (onglet === 'sat') await get('/sats', d => { dessinerCiel(d.sats); dessinerBarres(d.sats); });
  if (onglet === 'trk') await get('/track.json', dessinerTrace);
  if (onglet === 'pos') await get('/proches', majProches);
  if (onglet === 'net') await get('/wifi/etat', majReseau);
  } finally { ongletOccupe = false; }
}

let tickOccupe = false, prochainTick;
async function tick(){
  if (tickOccupe) return;
  clearTimeout(prochainTick);
  if (document.hidden) { prochainTick = setTimeout(tick, 2000); return; }
  tickOccupe = true;
  try {
    maj(await requete('/api'));
    await rafraichirOnglet();
  } catch(e) {
    $('connexion').textContent='Carte injoignable · données affichées périmées';
    $('etat').className='pill ko';
    $('etat').textContent="carte injoignable";
  } finally {
    tickOccupe = false;
    prochainTick = setTimeout(tick, 1000);
  }
}

$('depart').onclick = () => actionSortie('/sortie/debut');
$('sortiePause').onclick = () => actionSortie('/trace/pause');
$('terminer').onclick = () => { if(confirm('Terminer et sauvegarder cette sortie ?')) actionSortie('/sortie/fin'); };
$('raz').onclick  = () => { if(confirm('Effacer uniquement l’aperçu et ses statistiques ? Les sorties sauvegardées sont conservées.')) post('/reset', () => { tick(); }); };
$('simu').onclick = () => post('/simu', () => { tick(); });
$('pause').onclick = () => post('/trace/pause', () => { tick(); });
document.querySelectorAll('#profils button').forEach(b => {
  b.onclick = () => fetch('/profil',{method:'POST',cache:'no-store',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'p='+encodeURIComponent(b.dataset.p)}).then(r=>r.json()).then(()=>tick()).catch(()=>signaler('profil refuse'));
});

tick();
document.addEventListener('visibilitychange', () => { if (!document.hidden) tick(); });
</script>
</body>
</html>
)PAGE";
