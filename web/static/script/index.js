const countersocket = new WebSocket((window.location.origin+"/d/playercount").replace(/^https(.*)/, 'wss$1'));
countersocket.onmessage = function(event) {
    document.getElementById("counterDiv").innerText = event.data;
}
