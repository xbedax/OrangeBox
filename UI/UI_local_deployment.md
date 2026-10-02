Local mockup deployment:


1. Create the following structure somwhere at your local storage using files form github:

	UI
		camera_mockup.png
		door_closed.png
		door_open.png
		favicon.ico
		index.html
		light_off.png
		light_on.png
		./js:
			clicktable.js
			modal.js
			pingenerator.js
			websoc.js
		./scss:
			pico.css

	No special treatment necessary the files are just read by browser. This is complete web application tree. 

2. Open the websoc.js file in editor and adjust line 4 accordingly to your needs.

3. Put file /test/websocket_server.pl somewhere on your local storage and run it.

Depending on your OS installation you may need to adjust perl interpretter location, or simply make perl to run the script as usually.
There are some parameters available to modify responder behaviour, please see to comments at the beginning of the script. 
Make sure the location / listenning port of the responder corresponds to line 4 of websocket.pl file.

4. Open the index.html file in your favorite browser and enjoy :-)
