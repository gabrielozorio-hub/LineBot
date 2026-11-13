// Pagina web do painel (HTML + CSS + JS em um unico texto, guardado na flash)
#pragma once

const char PAGINA[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="pt-br"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>Seguidor de Linha</title>
<link href="https://fonts.googleapis.com/css2?family=Poppins:wght@400;600&display=swap" rel="stylesheet">
<style>
*{box-sizing:border-box;margin:0}
body{font-family:Poppins,system-ui,sans-serif;background:#fff;color:#111;max-width:760px;margin:auto;padding:16px;-webkit-tap-highlight-color:transparent}
header{display:flex;justify-content:space-between;align-items:center;gap:8px;margin-bottom:14px}
h1{font-size:18px;font-weight:600}
small,.mut{color:#777;font-size:12px}
nav{display:flex;gap:6px;margin-bottom:14px;overflow-x:auto}
button{font:inherit;font-size:14px;border:1.5px solid #111;background:#fff;color:#111;border-radius:10px;padding:8px 14px;cursor:pointer}
button.on,.stop{background:#111;color:#fff}
.stop{font-weight:600;letter-spacing:.05em}
.card{border:1.5px solid #111;border-radius:14px;padding:14px;margin-bottom:12px}
.card h2{font-size:12px;font-weight:600;text-transform:uppercase;letter-spacing:.08em;margin-bottom:10px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:12px}
.grid>.card{margin:0}
.big{font-size:28px;font-weight:600;line-height:1.15}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
.bar{height:8px;border:1.5px solid #111;border-radius:6px;margin-top:8px;overflow:hidden}
.bar i{display:block;height:100%;background:#111;transition:width .3s}
#sens{display:grid;grid-template-columns:repeat(8,1fr);gap:4px}
#sens div{aspect-ratio:1;border:1.5px solid #111;border-radius:6px}
#pos{position:relative;height:14px;margin:6px 0}
#pos i{position:absolute;top:0;width:3px;height:14px;background:#111;transition:left .1s}
.keys{display:grid;grid-template-columns:repeat(3,64px);gap:8px;justify-content:center}
.keys b{display:flex;align-items:center;justify-content:center;height:64px;border:1.5px solid #111;border-radius:10px;font-size:20px}
.keys b.on{background:#111;color:#fff}
.pad{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;max-width:320px;margin:auto;touch-action:none;user-select:none;-webkit-user-select:none}
.pad button{aspect-ratio:1;font-size:26px;padding:0}
.pad button.on{background:#111;color:#fff}
label{display:flex;justify-content:space-between;margin:14px 0 4px;font-size:14px}
input[type=range]{width:100%;accent-color:#111}
select{width:100%;padding:8px;border:1px solid #ddd;border-radius:8px;background:#fff;font:inherit}
details{border-top:1px solid #ddd;padding:10px 0}
summary{cursor:pointer;font-weight:600;font-size:14px}
.log{font-family:ui-monospace,Menlo,monospace;font-size:12px;display:grid;grid-template-columns:auto auto 1fr;gap:3px 12px;margin-top:8px}
.log span:nth-child(3n+2){color:#777}
.hide{display:none!important}
#toast{position:fixed;bottom:16px;left:50%;transform:translateX(-50%);background:#111;color:#fff;padding:8px 14px;border-radius:10px;font-size:13px;opacity:0;transition:opacity .2s;pointer-events:none}
</style></head><body>

<header><div><h1>Seguidor de Linha</h1><small id="st">conectando…</small></div>
<button class="stop" onclick="parar()">PARAR</button></header>

<nav><button data-t="painel" class="on">Painel</button><button data-t="controle">Controle</button><button data-t="ajustes">Ajustes</button><button data-t="logs">Logs</button></nav>

<section id="painel">
 <div class="card"><h2>Modo</h2><div class="row" id="modos">
  <button onclick="modo(0)">Parado</button><button onclick="modo(1)">Automático</button>
  <button onclick="modo(2)">Manual</button><button onclick="modo(3)">Calibrar</button></div></div>
 <div class="card"><h2>Sensores ao vivo</h2><div id="sens"></div><div id="pos"><i></i></div><small id="lin">-</small>
  <div id="raw" class="mut" style="display:grid;grid-template-columns:repeat(8,1fr);gap:4px;text-align:center;font-size:11px;margin-top:6px"></div>
  <small>Leitura bruta em µs: branco = número baixo, preto = número alto (até 3000). Depois de mudar a altura da barra, rode Calibrar passando os sensores sobre a linha e o chão.</small></div>
 <div class="grid" style="margin-bottom:12px">
  <div class="card"><h2>Bateria</h2><div class="big" id="bp">--</div><small id="bv">-</small><div class="bar"><i id="bb" style="width:0"></i></div></div>
  <div class="card"><h2>Volta atual</h2><div class="big" id="va">--</div><small id="uv">-</small></div>
  <div class="card"><h2>Recorde</h2><div class="big" id="rec">--</div><small id="nv">-</small></div>
 </div>
 <div class="card"><h2>Odômetro vitalício <span class="mut" style="text-transform:none;letter-spacing:0">(conta só na bateria)</span></h2><div class="grid">
  <div><div class="big" id="bo">-</div><small>vezes ligado</small></div>
  <div><div class="big" id="tl">-</div><small>tempo ligado</small></div>
  <div><div class="big" id="ta">-</div><small>tempo andando</small></div>
  <div><div class="big" id="tv">-</div><small>voltas no total</small></div></div></div>
</section>

<section id="controle" class="hide">
 <div class="card"><div class="row" style="justify-content:space-between"><h2 style="margin:0">Controle remoto</h2>
  <div class="row"><button id="bpc" onclick="disp('pc')">PC</button><button id="bcel" onclick="disp('cel')">Celular</button></div></div></div>
 <div class="card" id="cpc"><div class="keys"><span></span><b data-k="w">W</b><span></span><b data-k="a">A</b><b data-k="s">S</b><b data-k="d">D</b></div>
  <p class="mut" style="text-align:center;margin-top:12px">Segure W A S D para andar e solte para parar. Espaço = parar.</p></div>
 <div class="card" id="ccel"><div class="pad">
  <button data-d="q">↖</button><button data-d="f">↑</button><button data-d="e">↗</button>
  <button data-d="l">←</button><button data-d="s">■</button><button data-d="r">→</button>
  <button data-d="z">↙</button><button data-d="b">↓</button><button data-d="c">↘</button></div></div>
 <div class="card"><label>Velocidade manual <b id="vmv"></b></label><input type="range" id="vm" min="80" max="255" step="5"></div>
</section>

<section id="ajustes" class="hide">
 <div class="card"><h2>Direção (modo automático)</h2>
  <label>Velocidade <b id="vbv"></b></label><input type="range" id="vb" min="60" max="255" step="5">
  <label>Força da curva (%) <b id="cvv"></b></label><input type="range" id="cv" min="0" max="100" step="5">
  <small style="display:block;margin-top:4px">Quanto ele luta para trazer a linha de volta aos sensores do meio. Indo para a ponta, a roda de fora acelera e a de dentro freia; em 100 a de dentro gira para trás com tudo. Se ele zigue-zagueia, diminua. Se a linha escapa pela ponta, aumente.</small>
  <label>Folga no centro <b id="rtv"></b></label><input type="range" id="rt" min="0" max="1500" step="50">
  <small style="display:block;margin-top:4px">Faixa no meio da barra em que os dois motores ficam iguais. 0 = corrige sempre. 1000 = um sensor para cada lado.</small>
  <label>Lado da direção <b id="invv"></b></label><select id="inv"><option value="0">Normal</option><option value="1">Invertido</option></select>
  <small style="display:block;margin-top:4px">Normal: linha nos sensores da direita, ele vira para a esquerda (e vice-versa). Se fizer o contrário, troque para Invertido.</small>
  <p class="mut" style="margin-top:12px">Vale na hora e fica salvo na memória flash.</p></div>
 <div class="card"><h2>Motores</h2>
  <label>Balanço (%) <b id="balv"></b></label><input type="range" id="bal" min="-50" max="50" step="1">
  <small style="display:block;margin-top:4px">Se no manual, segurando W, ele puxa para a esquerda, o motor direito é mais forte: aumente (tira força do direito). Puxando para a direita, diminua (tira do esquerdo).</small>
  <label>Força mínima <b id="pmv"></b></label><input type="range" id="pm" min="0" max="200" step="5">
  <small style="display:block;margin-top:4px">Menor força que faz a roda girar. Se numa curva uma roda fica parada só chiando, aumente.</small></div>
 <div class="card"><h2>Teste de motores</h2>
  <p class="mut" style="margin-bottom:10px">Com o carrinho suspenso. Cada botão gira por 2 s na força máxima.</p>
  <div class="row"><button onclick="testar(255,0)">Esquerdo ↑</button><button onclick="testar(-255,0)">Esquerdo ↓</button>
  <button onclick="testar(0,255)">Direito ↑</button><button onclick="testar(0,-255)">Direito ↓</button>
  <button onclick="testar(255,255)">Os dois ↑</button></div></div>
 <div class="card"><h2>Zerar contadores</h2><div class="row"><button onclick="zerarRecorde()">Zerar recorde</button><button onclick="zerarOdometro()">Zerar odômetro</button></div></div>
</section>

<section id="logs" class="hide">
 <div class="card">
  <div class="row" id="filtros"><button data-c="" class="on">Tudo</button><button data-c="SIS">Sistema</button><button data-c="COR">Corrida</button><button data-c="PID">Ajustes</button><button data-c="BAT">Bateria</button><button data-c="CON">Conexão</button></div>
  <div class="row" style="margin-top:10px"><button onclick="carregarLogs()">Atualizar</button><button onclick="baixar()">Baixar .txt</button><button onclick="apagar()">Apagar</button></div>
  <div id="ls" style="margin-top:10px"></div></div>
</section>

<div id="toast"></div>

<script>
const $=id=>document.getElementById(id);
const MOD=['Parado','Automático','Manual','Calibrando'];
const ETAPA=['aguardando linha','seguindo a linha','dando ré','centralizando'];
const CAT={SIS:'Sistema',COR:'Corrida',PID:'Ajustes',BAT:'Bateria',CON:'Conexão'};
let aba='painel',S={};

function toast(t){const e=$('toast');e.textContent=t;e.style.opacity=1;clearTimeout(e.t);e.t=setTimeout(()=>e.style.opacity=0,1500)}
const get=u=>fetch(u).then(r=>{if(!r.ok)throw r;return r});
const virg=(n,c)=>n.toFixed(c).replace('.',',');
const seg=ms=>ms?virg(ms/1000,1)+' s':'--';
const tempo=s=>{const h=Math.floor(s/3600),m=Math.floor(s%3600/60);return h?h+'h '+m+'min':m+'min '+(s%60)+'s'};
const esc=t=>t.replace(/[&<>]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;'}[c]));

// abas
document.querySelectorAll('nav button').forEach(b=>b.onclick=()=>{
 aba=b.dataset.t;
 document.querySelectorAll('nav button').forEach(x=>x.classList.toggle('on',x==b));
 document.querySelectorAll('section').forEach(s=>s.classList.toggle('hide',s.id!=aba));
 if(aba=='logs')carregarLogs();
 if(aba!='controle')mover('s');
});

function modo(m){get('/m?v='+m).then(()=>toast(MOD[m]))}
function parar(){mover('s');modo(0)}

// status ao vivo
for(let i=0;i<8;i++)$('sens').appendChild(document.createElement('div'));
let ocupado=false;
function status(){
 // enquanto dirige, o status pausa para os comandos chegarem sem atraso
 if(ocupado||document.hidden||aba=='logs'||aba=='ajustes'||dir!='s')return;
 ocupado=true;
 get('/s').then(r=>r.json()).then(d=>{
  S=d;
  $('st').textContent=MOD[d.m]+(d.m==1?' ('+ETAPA[d.sg]+')':'')+' · '+(d.fb?'na bateria':'só no USB')+' · sessão #'+d.se+' · '+d.cl+' conectado(s)';
  [...$('modos').children].forEach((b,i)=>b.classList.toggle('on',i==d.m));
  [...$('sens').children].forEach((e,i)=>e.style.background='rgba(17,17,17,'+d.s[i]/1000+')');
  $('raw').innerHTML=d.r.map((v,i)=>'<span>D'+(i+1)+'<br>'+v+'</span>').join('');
  document.querySelector('#pos i').style.left='calc('+d.x/70+'% - 1px)';
  $('lin').textContent=d.l?'Linha na posição '+d.x+' (centro = 3500)':'Linha não encontrada';
  if(d.v<0){$('bp').textContent='--';$('bv').textContent='divisor de tensão não ligado'}
  else{$('bp').textContent=d.p+'%';$('bv').textContent=virg(d.v,2)+' V';$('bb').style.width=d.p+'%'}
  $('va').textContent=seg(d.va);$('uv').textContent='última: '+seg(d.uv);
  $('rec').textContent=seg(d.rec);$('nv').textContent=d.nv+' volta(s) nesta sessão';
  $('bo').textContent=d.bo;$('tl').textContent=tempo(d.tl);$('ta').textContent=tempo(d.ta);$('tv').textContent=d.tv;
 }).catch(()=>$('st').textContent='sem conexão com o carrinho').finally(()=>ocupado=false);
}
setInterval(status,250);
get('/t?e='+Math.floor(Date.now()/1000)).catch(()=>{});

// controle remoto: repete o comando enquanto segura; o carrinho para sozinho se parar de receber
let dir='s',rep,enviando=false;
// um comando por vez: o servidor do ESP32 atende uma conexao de cada vez
function enviar(d,forcar){
 if(enviando&&!forcar)return;
 enviando=true;
 fetch('/c?d='+d).catch(()=>{}).finally(()=>enviando=false);
}
function mover(d){
 if(d==dir)return;
 dir=d;enviar(d,true);clearInterval(rep);
 if(d!='s')rep=setInterval(()=>enviar(dir),200);
 document.querySelectorAll('.pad button').forEach(b=>b.classList.toggle('on',b.dataset.d==d&&d!='s'));
}
const tecla={};
function dirTeclas(){
 const f=tecla.w&&!tecla.s,b=tecla.s&&!tecla.w,l=tecla.a&&!tecla.d,r=tecla.d&&!tecla.a;
 return f?(l?'q':r?'e':'f'):b?(l?'z':r?'c':'b'):l?'l':r?'r':'s';
}
function teclas(e,v){
 if(aba!='controle')return;
 const k=e.key.toLowerCase();
 if(k==' '){e.preventDefault();parar();return}
 if(!'wasd'.includes(k)||k.length!=1)return;
 e.preventDefault();tecla[k]=v;
 document.querySelector('[data-k='+k+']').classList.toggle('on',v);
 mover(dirTeclas());
}
addEventListener('keydown',e=>teclas(e,true));
addEventListener('keyup',e=>teclas(e,false));
addEventListener('blur',()=>{for(const k in tecla)tecla[k]=false;document.querySelectorAll('.keys b').forEach(b=>b.classList.remove('on'));mover('s')});
document.querySelectorAll('.pad button').forEach(b=>{
 b.onpointerdown=e=>{e.preventDefault();b.setPointerCapture(e.pointerId);mover(b.dataset.d)};
 b.onpointerup=b.onpointercancel=()=>mover('s');
 b.oncontextmenu=e=>e.preventDefault();
});
function disp(t){
 $('cpc').classList.toggle('hide',t!='pc');$('ccel').classList.toggle('hide',t!='cel');
 $('bpc').classList.toggle('on',t=='pc');$('bcel').classList.toggle('on',t=='cel');
 try{localStorage.disp=t}catch(e){}
}
let pref;try{pref=localStorage.disp}catch(e){}
disp(pref||(matchMedia('(pointer:coarse)').matches?'cel':'pc'));

// ajustes salvos na flash
const CFG=['vb','rt','cv','bal','pm','vm','inv'];
const mostra=n=>$(n+'v').textContent=n=='inv'?'':$(n).value;
let tcfg;
CFG.forEach(n=>$(n).oninput=$(n).onchange=()=>{
 mostra(n);clearTimeout(tcfg);
 tcfg=setTimeout(()=>get('/p?'+CFG.map(x=>x+'='+$(x).value).join('&')).then(()=>toast('Salvo na flash')),300);
});
get('/p').then(r=>r.json()).then(c=>CFG.forEach(n=>{$(n).value=c[n];mostra(n)})).catch(()=>{});
function zerarRecorde(){if(confirm('Zerar o recorde de volta?'))get('/rr').then(()=>toast('Recorde zerado'))}
function testar(e,d){get('/mt?e='+e+'&d='+d).then(()=>toast('Girando por 2 s'))}
function zerarOdometro(){if(confirm('Zerar vezes ligado, tempos e voltas totais?'))get('/zo').then(()=>toast('Odômetro zerado'))}

// logs: agrupados por sessao (cada vez que ligou) e filtrados por assunto
let LOG=[],filtro='';const abertas=new Set();
document.querySelectorAll('#filtros button').forEach(b=>b.onclick=()=>{
 filtro=b.dataset.c;
 document.querySelectorAll('#filtros button').forEach(x=>x.classList.toggle('on',x==b));
 desenhar();
});
$('ls').addEventListener('toggle',e=>{const s=+e.target.dataset.s;e.target.open?abertas.add(s):abertas.delete(s)},true);
function carregarLogs(){
 get('/l').then(r=>r.text()).then(t=>{
  LOG=t.split('\n').filter(Boolean).map(l=>{const p=l.split('|');return{s:+p[0],ms:+p[1],c:p[2],m:p.slice(3).join('|')}});
  desenhar();
 }).catch(()=>toast('Não foi possível ler os logs'));
}
function sessoes(){
 const g={};LOG.forEach(e=>(g[e.s]=g[e.s]||[]).push(e));
 return Object.keys(g).map(Number).sort((a,b)=>b-a).map(id=>{
  const ev=g[id],sy=ev.find(e=>/@\d+/.test(e.m));
  const base=sy?+sy.m.match(/@(\d+)/)[1]*1000-sy.ms:0;
  return{id,ev,base};
 });
}
function hora(ms,base){
 if(base)return new Date(base+ms).toLocaleTimeString('pt-BR');
 const s=Math.floor(ms/1000);return '+'+String(Math.floor(s/60)).padStart(2,'0')+':'+String(s%60).padStart(2,'0');
}
const quando=x=>x.base?new Date(x.base+x.ev[0].ms).toLocaleString('pt-BR',{day:'2-digit',month:'2-digit',hour:'2-digit',minute:'2-digit'}):'sem horário';
const texto=m=>m.replace(/ @\d+/,'');
function desenhar(){
 const ss=sessoes();
 if(!ss.length){$('ls').innerHTML='<p class="mut">Nenhum log ainda.</p>';return}
 if(!abertas.size)abertas.add(ss[0].id);
 $('ls').innerHTML=ss.map(x=>{
  const lin=x.ev.filter(e=>!filtro||e.c==filtro).map(e=>'<span>'+hora(e.ms,x.base)+'</span><span>'+(CAT[e.c]||e.c)+'</span><span>'+esc(texto(e.m))+'</span>').join('');
  return '<details data-s="'+x.id+'"'+(abertas.has(x.id)?' open':'')+'><summary>Sessão #'+x.id+' · '+quando(x)+(x.id==S.se?' (atual)':'')+' · '+x.ev.length+' eventos</summary>'
   +(lin?'<div class="log">'+lin+'</div>':'<p class="mut" style="margin-top:8px">Nada neste filtro.</p>')+'</details>';
 }).join('');
}
function baixar(){
 let t='LOGS DO CARRINHO SEGUIDOR DE LINHA\n';
 sessoes().forEach(x=>{t+='\n== Sessão #'+x.id+' · '+quando(x)+' ==\n';x.ev.forEach(e=>t+=hora(e.ms,x.base)+'  '+(CAT[e.c]||e.c).padEnd(8)+'  '+texto(e.m)+'\n')});
 const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([t],{type:'text/plain'}));a.download='logs_carrinho.txt';a.click();
}
function apagar(){if(confirm('Apagar todos os logs da memória flash?'))get('/lc').then(()=>{abertas.clear();carregarLogs();toast('Logs apagados')})}
</script>
</body></html>)rawliteral";
