var lobbyKey = document.body.dataset.lobbykey;
document.getElementById('gamelink').setAttribute('value', window.location.origin+'/'+lobbyKey);
document.getElementById('copybutton').addEventListener("click", function() {
    var copyText = document.getElementById('gamelink');
    copyText.select();
    copyText.setSelectionRange(0,99999);
    document.execCommand("copy");
});
document.forms['playprivateform'].addEventListener('submit', function (event) {
    event.preventDefault();
    window.location.href = '/'+lobbyKey;
});
