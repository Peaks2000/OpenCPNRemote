#include "web_assets.h"

const char* WebIndexHtml() {
  return R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>OpenCPN Remote</title>
<style>
html,body{margin:0;width:100%;height:100%;background:#050608;overflow:hidden;touch-action:none;-webkit-tap-highlight-color:transparent;-webkit-touch-callout:none;overscroll-behavior:none}
#stage{position:fixed;inset:0;background:#050608;overflow:hidden}
#view{position:absolute;left:0;top:0;width:auto;height:auto;max-width:none;max-height:none;image-rendering:auto;user-select:none;-webkit-user-select:none;-webkit-user-drag:none}
#state{position:fixed;left:10px;bottom:10px;padding:5px 8px;border-radius:6px;background:rgba(0,0,0,.58);color:#fff;font:12px system-ui,sans-serif}
#tools{position:fixed;right:10px;top:10px;display:flex;gap:6px}
#tools button{width:38px;height:34px;border:0;border-radius:6px;background:rgba(0,0,0,.6);color:#fff;font:18px system-ui,sans-serif}
@media (max-width:700px),(pointer:coarse){#tools{display:none}#state{left:8px;bottom:calc(env(safe-area-inset-bottom) + 8px);opacity:.72}}
#auth{position:fixed;inset:0;display:none;place-items:center;background:#101418;color:#eef3f6;font:16px system-ui,sans-serif}
#auth form{width:min(340px,calc(100vw - 32px));display:grid;gap:10px}
#auth input,#auth button{font:inherit;padding:10px;border-radius:6px;border:1px solid #3b464d}
#auth button{background:#1d6f94;color:white;border:0}
#takeover{position:fixed;right:10px;bottom:10px;display:none;padding:8px 10px;border:0;border-radius:6px;background:#1d6f94;color:#fff;font:14px system-ui,sans-serif}
</style>
</head>
<body>
<div id="stage"><img id="view" alt=""></div>
<div id="state">connecting</div>
<div id="tools"><button id="zoomOut">-</button><button id="fit">1x</button><button id="zoomIn">+</button></div>
<div id="auth"><form><strong>OpenCPN Remote password</strong><input id="password" type="password" autocomplete="current-password"><button>Connect</button></form></div>
<button id="takeover">Resume</button>
<script>
const token=new URLSearchParams(location.search).get("token")||"";
const passwordRequired=false;
let remotePassword=sessionStorage.ocpnRemotePassword||"";
const clientId=sessionStorage.ocpnRemoteClientId||(sessionStorage.ocpnRemoteClientId=(crypto.randomUUID?crypto.randomUUID():String(Date.now())+Math.random()));
const img=document.getElementById("view");
const state=document.getElementById("state");
const auth=document.getElementById("auth");
const passwordInput=document.getElementById("password");
const takeover=document.getElementById("takeover");
const zoomIn=document.getElementById("zoomIn");
const zoomOut=document.getElementById("zoomOut");
const fit=document.getElementById("fit");
let natural={w:1,h:1};
let busy=false,lastMove=0,pendingMove=null,clientActive=true,claimNext=true;
let pinch=null,navGesture=null,activeTouchId=null,touchDragging=false,inputSeq=Date.now(),touchDrag=null,lastTouchPoint=null,viewX=0,viewY=0,viewScale=1,viewCentered=false,lastNatural="0x0";
function authReady(){return !passwordRequired||remotePassword.length>0}
auth.querySelector("form").addEventListener("submit",e=>{e.preventDefault();remotePassword=passwordInput.value;sessionStorage.ocpnRemotePassword=remotePassword;auth.style.display="none";heartbeat();tick()});
takeover.addEventListener("click",()=>{claimNext=true;takeover.style.display="none";heartbeat();tick()});
if(!authReady())auth.style.display="grid";
function centerPoint(){return mapPointXY(window.innerWidth/2,window.innerHeight/2)}
zoomIn.addEventListener("click",()=>send("/input/pointer",{type:"wheel",delta:120,...centerPoint()}));
zoomOut.addEventListener("click",()=>send("/input/pointer",{type:"wheel",delta:-120,...centerPoint()}));
fit.addEventListener("click",()=>{viewScale=1;centerView()});
function frameUrl(){return `/frame?token=${encodeURIComponent(token)}&client=${encodeURIComponent(clientId)}&t=${Date.now()}`}
async function tick(){
  if(!authReady())return;
  if(busy)return;
  busy=true;
  const next=new Image();
  next.onload=()=>{natural={w:next.naturalWidth||1,h:next.naturalHeight||1};const key=natural.w+"x"+natural.h;img.src=next.src;img.style.width=natural.w+"px";img.style.height=natural.h+"px";if(!viewCentered||key!==lastNatural){centerView();viewCentered=true;lastNatural=key}else clampView();state.textContent="live";busy=false;setTimeout(tick,0)};
  next.onerror=()=>{state.textContent=state.textContent==="kicked off"?"kicked off":"waiting";busy=false;setTimeout(tick,160)};
  next.src=frameUrl();
}
function mapPointXY(clientX,clientY){
  const r=img.getBoundingClientRect();
  const inside=clientX>=r.left&&clientX<=r.right&&clientY>=r.top&&clientY<=r.bottom;
  return {
    x:Math.round(Math.max(0,Math.min(natural.w-1,(clientX-r.left)/viewScale))),
    y:Math.round(Math.max(0,Math.min(natural.h-1,(clientY-r.top)/viewScale))),
    inside
  };
}
function mapPoint(e){return mapPointXY(e.clientX,e.clientY)}
function canvasCenter(){return {x:Math.round(natural.w/2),y:Math.round(natural.h/2)}}
function minViewScale(){return Math.max(.28,Math.min(1,window.innerWidth/natural.w,window.innerHeight/natural.h))}
function isMobileView(){return matchMedia("(max-width:700px), (pointer:coarse)").matches}
function centerView(){
  if(isMobileView())viewScale=minViewScale();
  viewX=Math.round((window.innerWidth-natural.w)/2);
  viewY=Math.round((window.innerHeight-natural.h)/2);
  clampView();
}
function clampView(){
  if(!isMobileView())viewScale=1;
  const scaledW=natural.w*viewScale,scaledH=natural.h*viewScale;
  const fitScale=isMobileView()?minViewScale():1;
  const fitted=Math.abs(viewScale-fitScale)<.02;
  if(fitted&&scaledW<=window.innerWidth)viewX=Math.round((window.innerWidth-scaledW)/2);
  else if(scaledW<=window.innerWidth)viewX=Math.max(0,Math.min(window.innerWidth-scaledW,viewX));
  else viewX=Math.max(window.innerWidth-scaledW,Math.min(0,viewX));
  if(fitted&&scaledH<=window.innerHeight)viewY=Math.round((window.innerHeight-scaledH)/2);
  else if(scaledH<=window.innerHeight)viewY=Math.max(0,Math.min(window.innerHeight-scaledH,viewY));
  else viewY=Math.max(window.innerHeight-scaledH,Math.min(0,viewY));
  img.style.transformOrigin="0 0";
  img.style.transform=`translate(${Math.round(viewX)}px,${Math.round(viewY)}px) scale(${viewScale})`;
}
function setViewScale(next,anchorX=window.innerWidth/2,anchorY=window.innerHeight/2){
  if(!isMobileView()){viewScale=1;clampView();return}
  const old=viewScale;
  viewScale=Math.max(minViewScale(),Math.min(2.75,next));
  const imgX=(anchorX-viewX)/old;
  const imgY=(anchorY-viewY)/old;
  viewX=anchorX-imgX*viewScale;
  viewY=anchorY-imgY*viewScale;
  clampView();
}
function clampCanvasPoint(x,y){
  return {
    x:Math.round(Math.max(0,Math.min(natural.w-1,x))),
    y:Math.round(Math.max(0,Math.min(natural.h-1,y)))
  };
}
function send(path,body){
  fetch(`${path}?token=${encodeURIComponent(token)}&client=${encodeURIComponent(clientId)}`,{method:"POST",headers:{"content-type":"application/json"},body:JSON.stringify({seq:++inputSeq,...body})}).catch(()=>{});
}
function sendMove(e){
  const p=mapPoint(e);
  if(!p.inside)return;
  pendingMove={type:"move",x:p.x,y:p.y};
  const now=performance.now();
  if(now-lastMove<16)return;
  lastMove=now;
  const body=pendingMove;
  pendingMove=null;
  send("/input/pointer",body);
}
function sendMovePoint(p){
  pendingMove={type:"move",x:p.x,y:p.y};
  const now=performance.now();
  if(now-lastMove<16)return;
  lastMove=now;
  const body=pendingMove;
  pendingMove=null;
  send("/input/pointer",body);
}
function heartbeat(){
  if(!authReady())return;
  const claim=claimNext;
  fetch(`/session?token=${encodeURIComponent(token)}&client=${encodeURIComponent(clientId)}`,{method:"POST",headers:{"content-type":"application/json"},body:JSON.stringify({width:window.innerWidth,height:window.innerHeight,dpr:window.devicePixelRatio||1,password:remotePassword,claim})})
    .then(r=>{if(r.status===409){clientActive=false;claimNext=false;state.textContent="paused";takeover.style.display="block"}else if(r.status===403){clientActive=false;state.textContent="password required";sessionStorage.removeItem("ocpnRemotePassword");remotePassword="";auth.style.display="grid"}else if(r.ok){clientActive=true;claimNext=false;takeover.style.display="none";if(state.textContent==="paused"||state.textContent==="kicked off")state.textContent="live"}})
    .catch(()=>{})
}
setInterval(heartbeat,1000);
window.addEventListener("resize",heartbeat);
window.addEventListener("resize",clampView);
heartbeat();
function isTouchPointer(e){return e.pointerType==="touch"||e.pointerType==="pen"}
window.addEventListener("pointerdown",e=>{if(isTouchPointer(e)){e.preventDefault();return}if(clientActive&&!pinch){const p=mapPoint(e);if(p.inside){img.setPointerCapture?.(e.pointerId);send("/input/pointer",{type:"down",x:p.x,y:p.y})}}e.preventDefault()},{passive:false});
window.addEventListener("pointermove",e=>{if(isTouchPointer(e)){e.preventDefault();return}if(clientActive&&!pinch&&e.buttons)sendMove(e);e.preventDefault()},{passive:false});
window.addEventListener("pointerup",e=>{if(isTouchPointer(e)){e.preventDefault();return}if(clientActive&&!pinch){const p=mapPoint(e);if(p.inside)send("/input/pointer",{type:"up",x:p.x,y:p.y})}e.preventDefault()},{passive:false});
window.addEventListener("pointercancel",e=>{if(isTouchPointer(e)){e.preventDefault();return}if(clientActive){const p=mapPoint(e);if(p.inside)send("/input/pointer",{type:"up",x:p.x,y:p.y})}e.preventDefault()},{passive:false});
window.addEventListener("wheel",e=>{if(clientActive){const p=mapPoint(e);if(p.inside)send("/input/pointer",{type:"wheel",delta:-Math.sign(e.deltaY||0)*120,x:p.x,y:p.y})}e.preventDefault()},{passive:false});
window.addEventListener("keydown",e=>{if(clientActive)send("/input/key",{type:"keydown",keyCode:e.keyCode,key:e.key})});
window.addEventListener("keyup",e=>{if(clientActive)send("/input/key",{type:"keyup",keyCode:e.keyCode,key:e.key})});
function touchDist(a,b){return Math.hypot(a.clientX-b.clientX,a.clientY-b.clientY)}
function touchMid(a,b){return {x:(a.clientX+b.clientX)/2,y:(a.clientY+b.clientY)/2}}
function touchCenter(list){
  let x=0,y=0;
  for(const t of list){x+=t.clientX;y+=t.clientY}
  return {x:x/list.length,y:y/list.length};
}
function findTouch(list,id){for(const t of list){if(t.identifier===id)return t}return null}
function sendTap(p){
  send("/input/pointer",{type:"tap",x:p.x,y:p.y});
}
function endTouchDrag(t){
  if(clientActive&&touchDragging&&(!touchDrag||touchDrag.started)){
    const p=lastTouchPoint||(t?mapPointXY(t.clientX,t.clientY):null);
    if(p)send("/input/pointer",{type:"up",x:p.x,y:p.y});
  }
  activeTouchId=null;
  touchDragging=false;
  touchDrag=null;
  lastTouchPoint=null;
}
function sendOpenCpnZoom(delta,mid){
  if(!clientActive||!delta)return;
  let p=mapPointXY(mid.x,mid.y);
  if(!p.inside)p=centerPoint();
  const steps=Math.max(-3,Math.min(3,delta));
  send("/input/pointer",{type:"wheel",delta:steps*120,x:p.x,y:p.y});
}
window.addEventListener("touchstart",e=>{
  if(!clientActive){e.preventDefault();return}
  if(e.touches.length>=3){
    if(pinch&&pinch.dragging&&lastTouchPoint)send("/input/pointer",{type:"up",x:lastTouchPoint.x,y:lastTouchPoint.y});
    pinch=null;
    activeTouchId=null;
    touchDragging=false;
    touchDrag=null;
    lastTouchPoint=null;
    const c=touchCenter(e.touches);
    navGesture={x:c.x,y:c.y,startX:c.x,startY:c.y,viewX,viewY,moved:false};
  }else if(e.touches.length===1&&!pinch&&!navGesture&&!touchDragging){
    const t=e.touches[0];
    const p=mapPointXY(t.clientX,t.clientY);
    if(p.inside){
      activeTouchId=t.identifier;
      touchDragging=true;
      touchDrag={startX:t.clientX,startY:t.clientY,tapPoint:p,started:false};
      lastTouchPoint=p;
    }
  }else if(e.touches.length===2&&!navGesture){
    activeTouchId=null;
    touchDragging=false;
    touchDrag=null;
    lastTouchPoint=null;
    const m=touchMid(e.touches[0],e.touches[1]);
    const c=canvasCenter();
    pinch={dist:touchDist(e.touches[0],e.touches[1]),steps:0,x:m.x,y:m.y,startX:m.x,startY:m.y,centerX:c.x,centerY:c.y,dragging:false};
  }
  e.preventDefault();
},{passive:false});
window.addEventListener("touchmove",e=>{
  if(!clientActive){e.preventDefault();return}
  if(navGesture&&e.touches.length>=3){
    const c=touchCenter(e.touches);
    const dx=c.x-navGesture.startX;
    const dy=c.y-navGesture.startY;
    if(Math.hypot(dx,dy)>8)navGesture.moved=true;
    viewX=navGesture.viewX+dx;
    viewY=navGesture.viewY+dy;
    clampView();
  }else if(pinch&&e.touches.length===2){
    const m=touchMid(e.touches[0],e.touches[1]);
    const scale=touchDist(e.touches[0],e.touches[1])/Math.max(1,pinch.dist);
    const steps=Math.trunc(Math.log(scale)/Math.log(1.18));
    const delta=steps-pinch.steps;
    if(delta){pinch.steps=steps;sendOpenCpnZoom(delta,m)}
    const dx=(m.x-pinch.startX)/viewScale;
    const dy=(m.y-pinch.startY)/viewScale;
    if(!pinch.dragging&&Math.hypot(dx,dy)>6){
      pinch.dragging=true;
      lastTouchPoint={x:pinch.centerX,y:pinch.centerY};
      send("/input/pointer",{type:"down",x:pinch.centerX,y:pinch.centerY});
    }
    if(pinch.dragging){
      const p=clampCanvasPoint(pinch.centerX+dx,pinch.centerY+dy);
      lastTouchPoint=p;
      sendMovePoint(p);
    }
  }else if(touchDragging){
    const t=findTouch(e.touches,activeTouchId);
    if(t){
      touchDrag.lastX=t.clientX;
      touchDrag.lastY=t.clientY;
    }
  }
  e.preventDefault();
},{passive:false});
window.addEventListener("touchend",e=>{
  if(navGesture&&e.touches.length<3){
    if(!navGesture.moved){
      const fit=minViewScale();
      const next=viewScale<.95?1:(viewScale<1.75?1.75:(viewScale<2.35?2.5:fit));
      setViewScale(next,navGesture.x,navGesture.y);
      if(next===fit)centerView();
    }
    navGesture=null;
  }
  if(pinch&&e.touches.length<2){
    if(pinch.dragging&&lastTouchPoint)send("/input/pointer",{type:"up",x:lastTouchPoint.x,y:lastTouchPoint.y});
    pinch=null;
    lastTouchPoint=null;
  }
  if(touchDragging&&!findTouch(e.touches,activeTouchId)){
    if(touchDrag&&!touchDrag.started)sendTap(touchDrag.tapPoint);
    else endTouchDrag(e.changedTouches[0]);
    activeTouchId=null;
    touchDragging=false;
    touchDrag=null;
    lastTouchPoint=null;
  }
  e.preventDefault();
},{passive:false});
window.addEventListener("touchcancel",e=>{
  if(pinch&&pinch.dragging&&lastTouchPoint)send("/input/pointer",{type:"up",x:lastTouchPoint.x,y:lastTouchPoint.y});
  pinch=null;
  navGesture=null;
  endTouchDrag(e.changedTouches[0]);
  e.preventDefault();
},{passive:false});
tick();
</script>
</body>
</html>)HTML";
}

std::string WebIndexHtmlWithToken(const std::string& token,
                                  bool password_required) {
  std::string html = WebIndexHtml();
  const std::string needle =
      "const token=new URLSearchParams(location.search).get(\"token\")||\"\";";
  const std::string replacement =
      "const embeddedToken=\"" + token + "\";\n"
      "const token=new URLSearchParams(location.search).get(\"token\")||embeddedToken;";
  const size_t pos = html.find(needle);
  if (pos != std::string::npos) html.replace(pos, needle.size(), replacement);
  const std::string pw_needle = "const passwordRequired=false;";
  const size_t pw_pos = html.find(pw_needle);
  if (pw_pos != std::string::npos) {
    html.replace(pw_pos, pw_needle.size(),
                 std::string("const passwordRequired=") +
                     (password_required ? "true;" : "false;"));
  }
  return html;
}

const char* WebAccessHelpHtml() {
  return R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>OpenCPN Remote</title>
<style>
body{margin:0;min-height:100vh;display:grid;place-items:center;background:#101418;color:#eef3f6;font:16px system-ui,sans-serif}
main{width:min(620px,calc(100vw - 32px));line-height:1.45}
h1{font-size:24px;margin:0 0 12px}
p{margin:10px 0;color:#c9d4da}
code{display:block;margin:12px 0;padding:12px;background:#050708;border:1px solid #2a3338;border-radius:6px;color:#fff;overflow:auto}
</style>
</head>
<body>
<main>
<h1>OpenCPN Remote is running</h1>
<p>This page is locked because the access token is missing or wrong.</p>
<p>Open the URL saved by the plugin in <strong>opencpn_remote_url.txt</strong>, or look in the OpenCPN log for a line like:</p>
<code>OpenCPN Remote listening on http://0.0.0.0:8765/?token=...</code>
<p>From another device, replace <strong>0.0.0.0</strong> or <strong>127.0.0.1</strong> with this computer's LAN IP address, but keep the full <strong>?token=...</strong> part.</p>
</main>
</body>
</html>)HTML";
}
