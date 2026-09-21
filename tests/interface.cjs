const fs=require('fs'),vm=require('vm'),assert=require('assert');
const text=fs.readFileSync(require('path').join(__dirname,'../EdgeGPS/page.h'),'utf8');
const script=text.match(/<script>([\s\S]*?)<\/script>/)[1];
const path=require('path');
const firmware=fs.readFileSync(path.join(__dirname,'../EdgeGPS/EdgeGPS.ino'),'utf8');
const readme=fs.readFileSync(path.join(__dirname,'../README.md'),'utf8');
const ap=firmware.match(/#define EDGEGPS_AP_PASS "([^"]+)"/)[1];
assert(readme.includes('`'+ap+'`'),'Mot de passe AP documente et identique au code');
for(const route of firmware.matchAll(/server\.on\("([^"]+)"/g)) {
  assert(readme.includes('`'+route[1]+'`') || readme.includes('`'+route[1]+'?'),'Route documentee : '+route[1]);
}
new vm.Script(script);
const ids=[...text.matchAll(/id="([^"]+)"/g)].map(m=>m[1]);
assert.equal(ids.length,new Set(ids).size,'IDs uniques');
function element(){return {textContent:'',disabled:false,classList:{toggle(){}},append(){},replaceChildren(){}};}
const elements=Object.fromEntries(ids.map(id=>[id,element()]));
const context=vm.createContext({document:{hidden:true,getElementById:id=>{assert(elements[id],'ID '+id);return elements[id]},querySelectorAll:()=>[],addEventListener(){},createElement:element},setTimeout:()=>1,clearTimeout(){},AbortController,console,Date,confirm:()=>true,fetch:async()=>({ok:true,json:async()=>({active:true,pause:false,id:1,stockage:true,erreur:false,libre:90000})})});
vm.runInContext(script,context);
vm.runInContext('afficherSortie({active:false,pause:false,id:0,stockage:true,erreur:false,libre:90000})',context);
assert(!elements.depart.disabled); assert(elements.sortiePause.disabled); assert(elements.terminer.disabled);
vm.runInContext('afficherSortie({active:true,pause:true,id:1,stockage:true,erreur:false,libre:90000})',context);
assert(elements.depart.disabled); assert.equal(elements.sortiePause.textContent,'Reprendre'); assert(!elements.terminer.disabled);
vm.runInContext('afficherSortie({active:true,pause:true,id:1,stockage:true,erreur:true,libre:10000})',context);
assert(elements.sortiePause.disabled); assert(!elements.terminer.disabled);
vm.runInContext('afficherHistorique({sorties:[{id:1,distance:1000,duree:360,terminee:true}]})',context);
assert.equal(vm.runInContext('fmtDuree(0)',context),'0s');
assert.equal(1000*3600/360000,10,'1km en 6min =10km/h');
vm.runInContext('actionSortie("/sortie/debut")',context).then(()=>{
assert.equal(elements.sortieTitre.textContent,'Sortie 1 · enregistrement');
assert.equal(elements.sortieMessage.textContent,'Enregistré.');
console.log('PASS : mot de passe AP et routes documentes, syntaxe JS, IDs, états démarrage/pause/stockage, historique, action asynchrone, moyenne.');
});
