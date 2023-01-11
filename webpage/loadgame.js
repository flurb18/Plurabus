var gamePanelSize = 300;
var gameWindowPadding = 10;
var scale = 1;
while (scale*(gameSize) + gamePanelSize < window.innerWidth - gameWindowPadding &&
       scale*(gameSize*7/6) < window.innerHeight - gameWindowPadding) { scale++; }
scale--;
Module['arguments'] = [
    gameSize.toString(),
    gamePanelSize.toString(),
    scale.toString(),
    pstr.toString()
];
var mainscript = document.createElement('script');
mainscript.setAttribute('src','hivemindweb.js');
document.head.appendChild(mainscript);

