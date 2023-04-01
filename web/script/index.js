function buttonClick(actionType) {
    grecaptcha.enterprise.ready(async () => {
        const token = await grecaptcha.enterprise.execute('6LetnQQlAAAAABNjewyT0QnLyxOPkMharK-SILmD', {action: actionType});
	document.getElementById(actionType+"forminput").value = token;
	document.getElementById(actionType+"form").submit();
    });
}
document.getElementById('publicformbutton').onclick = function () { buttonClick("public") };
document.getElementById('privateformbutton').onclick = function () { buttonClick("private") };
window.setInterval(function () {
    var i = document.getElementById("counteriframe");
    i.src = i.src;
}, 30000);
