document.forms['publicform'].addEventListener('submit', function (event) {
    event.preventDefault();
    grecaptcha.enterprise.ready(async () => {
        const token = await grecaptcha.enterprise.execute('6LetnQQlAAAAABNjewyT0QnLyxOPkMharK-SILmD', {action: 'public'});
	window.location.href = "/action?a=public&t="+token;
    });
});
document.forms['privateform'].addEventListener('submit', function (event) {
    event.preventDefault();
    grecaptcha.enterprise.ready(async () => {
        const token = await grecaptcha.enterprise.execute('6LetnQQlAAAAABNjewyT0QnLyxOPkMharK-SILmD', {action: 'private'});
	window.location.href = "/action?a=private&t="+token;
    });
});
