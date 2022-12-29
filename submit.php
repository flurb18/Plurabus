<!DOCTYPE html>
<?php
 if ($_POST) {
 $pairstr = $_POST['pstr'].$_POST['gameSize'];
 $size = $_POST['gameSize'];
 } else {
   echo("No post data found :(");       
 }
?>
<html>

  <head>
    <meta charset="utf-8">
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
    <link rel="stylesheet" href="styles.css">
  </head>

  <body>
    <!-- Create the canvas that the C++ code will draw into -->
    <canvas id="canvas" oncontextmenu="event.preventDefault()"></canvas>

    <script>
      var gameSize = parseInt('<?php echo $size; ?>');
      var gamePanelSize = 240;
      var menuItems = 6;
      var unitLimit = 4096;
      var gameWindowPadding = 10;
      var dispSize = Math.min(window.innerWidth, window.innerHeight);
      var gameDispSize = dispSize;
      var i = 0;
      do {
	  gameDispSize = (dispSize - (2*gameWindowPadding) - i) * menuItems / (menuItems + 1);
	  if (gameDispSize % gameSize == 0) {
	      break;
	  }
	  i++;
      } while (gameDispSize > gameSize);
      var gameScale = gameDispSize / gameSize;
      var Module = {
          canvas: (function() { return document.getElementById('canvas'); })(),
	  arguments: [
	      gameSize.toString(),
	      gamePanelSize.toString(),
	      menuItems.toString(),
	      gameScale.toString(),
	      unitLimit.toString(),
	      '<?php echo $pairstr; ?>'
	  ]
      };
      </script>
    
    <script src="webhd.js"></script>
    
  </body>

</html>
