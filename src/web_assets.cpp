#include "web_assets.h"

const char* WebIndexHtml() {
  return R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>OpenCPN Remote</title>
<style>
html,body{margin:0;width:100%;height:100%;background:#050608;overflow:hidden;touch-action:none}
#stage{position:fixed;inset:0;display:grid;place-items:center;background:#050608}
#view{max-width:100vw;max-height:100vh;width:100vw;height:100vh;object-fit:contain;image-rendering:auto;user-select:none;-webkit-user-drag:none}
#state{position:fixed;left:10px;bottom:10px;padding:5px 8px;border-radius:6px;background:rgba(0,0,0,.58);color:#fff;font:12px system-ui,sans-serif}
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
let natural={w:1,h:1};
let busy=false,lastMove=0,pendingMove=null,clientActive=true,claimNext=true;
function authReady(){return !passwordRequired||remotePassword.length>0}
auth.querySelector("form").addEventListener("submit",e=>{e.preventDefault();remotePassword=passwordInput.value;sessionStorage.ocpnRemotePassword=remotePassword;auth.style.display="none";heartbeat();tick()});
takeover.addEventListener("click",()=>{claimNext=true;takeover.style.display="none";heartbeat();tick()});
if(!authReady())auth.style.display="grid";
function frameUrl(){return `/frame?token=${encodeURIComponent(token)}&client=${encodeURIComponent(clientId)}&t=${Date.now()}`}
async function tick(){
  if(!authReady())return;
  if(busy)return;
  busy=true;
  const next=new Image();
  next.onload=()=>{natural={w:next.naturalWidth||1,h:next.naturalHeight||1};img.src=next.src;state.textContent="live";busy=false;setTimeout(tick,0)};
  next.onerror=()=>{state.textContent=state.textContent==="kicked off"?"kicked off":"waiting";busy=false;setTimeout(tick,160)};
  next.src=frameUrl();
}
function mapPoint(e){
  const r=img.getBoundingClientRect();
  const scale=Math.min(r.width/natural.w,r.height/natural.h);
  const shownW=natural.w*scale, shownH=natural.h*scale;
  const ox=r.left+(r.width-shownW)/2, oy=r.top+(r.height-shownH)/2;
  return {x:Math.round((e.clientX-ox)/scale),y:Math.round((e.clientY-oy)/scale)};
}
function send(path,body){fetch(`${path}?token=${encodeURIComponent(token)}&client=${encodeURIComponent(clientId)}`,{method:"POST",headers:{"content-type":"application/json"},body:JSON.stringify(body)}).catch(()=>{})}
function sendMove(e){
  pendingMove={type:"move",...mapPoint(e)};
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
heartbeat();
window.addEventListener("pointerdown",e=>{if(clientActive){img.setPointerCapture?.(e.pointerId);send("/input/pointer",{type:"down",...mapPoint(e)})}e.preventDefault()},{passive:false});
window.addEventListener("pointermove",e=>{if(clientActive&&e.buttons)sendMove(e);e.preventDefault()},{passive:false});
window.addEventListener("pointerup",e=>{if(clientActive)send("/input/pointer",{type:"up",...mapPoint(e)});e.preventDefault()},{passive:false});
window.addEventListener("wheel",e=>{if(clientActive)send("/input/pointer",{type:"wheel",delta:e.deltaY,...mapPoint(e)});e.preventDefault()},{passive:false});
window.addEventListener("keydown",e=>{if(clientActive)send("/input/key",{type:"keydown",keyCode:e.keyCode,key:e.key})});
window.addEventListener("keyup",e=>{if(clientActive)send("/input/key",{type:"keyup",keyCode:e.keyCode,key:e.key})});
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
