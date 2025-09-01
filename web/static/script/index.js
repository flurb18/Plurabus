function setupButton(name) {
    document.getElementById(name + "formbutton").onclick = function () { document.getElementById(name + "form").submit() };
}
setupButton("public")
setupButton("private")
setupButton("practice")
const countersocket = new WebSocket((window.location.origin+"/d/playercount").replace(/^https(.*)/, 'wss$1'));
countersocket.onmessage = function(event) {
    document.getElementById("counterDiv").innerText = event.data;
}
