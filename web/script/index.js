function buttonClick(actionType) {
    grecaptcha.enterprise.ready(async () => {
        const token = await grecaptcha.enterprise.execute('6LetnQQlAAAAABNjewyT0QnLyxOPkMharK-SILmD', {action: actionType});
	document.getElementById(actionType+"forminput").value = token;
	document.getElementById(actionType+"form").submit();
    });
}
document.getElementById('numPlayers').setAttribute('value', document.body.dataset.numplayers);
document.getElementById('publicformbutton').onclick = function () { buttonClick("public") };
document.getElementById('privateformbutton').onclick = function () { buttonClick("private") };
