function buttonClick(actionType) {
    grecaptcha.enterprise.ready(async () => {
        const token = await grecaptcha.enterprise.execute('6LetnQQlAAAAABNjewyT0QnLyxOPkMharK-SILmD', {action: actionType});
	document.getElementById(actionType+"forminput").value = token;
	document.getElementById(actionType+"form").submit();
    });
}
document.getElementById('publicformbutton').onclick = function () { buttonClick("public") };
document.getElementById('privateformbutton').onclick = function () { buttonClick("private") };
const countersocket = new WebSocket("wss://plurabus.me/playercount");
countersocket.onmessage = function(event) {
    document.getElementById("counterDiv").innerText = event.data;
}
