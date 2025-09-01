function buttonClick(actionType) {
	document.getElementById(actionType+"form").submit();
}
document.getElementById('spawnerattackformbutton').onclick = function () { buttonClick("spawnerattack") };