<!DOCTYPE html>
<?php
 $pstr = $_POST['pstr'];
 $size = $_POST['gameSize'];
 if ($_POST) {
 if (preg_match('/^$|[a-zA-Z0-9]+/', $pstr)) {
 if (ctype_alnum($size)) {
 if (preg_match('/[0-9]+/', $size)) {
 $pairstr = $pstr.$size;
 } else {
 echo("size is not numeric");
 }} else {
 echo("size is not alphanumeric");
 }} else {
 echo("pstr is not alphanumeric");
 }
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
      var gameWindowPadding = 10;
      var scale = 1;
      while (scale*(gameSize) + gamePanelSize < window.innerWidth && scale*(gameSize*7/6) < window.innerHeight) {
	  scale++;
      }
      scale--;
      var Module = {
          canvas: (function() { return document.getElementById('canvas'); })(),
	  arguments: [
	      gameSize.toString(),
	      gamePanelSize.toString(),
	      scale.toString(),
	      '<?php echo $pairstr; ?>'
	  ]
      };
      </script>
    
    <script src="hivemindweb.js"></script>
    
  </body>

</html>
