// WebSocket config — adjust URL to your server
//const WS_URL = "wss://box.inforoom.cz";
var WS_URL = "wss://" + location.hostname + "/";
let ws = null;
var lastContact = 0;
var connectionState = 0;      //0 - disconnected, 1 - unknown, 2 - connected
var cumulatedDrift = 0;
var messagesReceived = 0;
const driftPoolLength = 10;
driftPool = new Array(driftPoolLength);
var driftPtr = 0;

// Actions on page load
//window.addEventListener('load', onLoad);
//function onLoad(event) {
//    getWebSocket();
//}

// Initialise (and reconnect) the WebSocket
const getWebSocket = () => {
  if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) {
    return ws;
  }
  ws = new WebSocket(WS_URL);
  ws.addEventListener("open",  () => {console.log("WS connected"); lastContact =  Date.now(); connectionState = 2;});
  ws.addEventListener("error", (e) => {console.error("WS error", e); connectionState = 1;});
  ws.addEventListener("close", () => {console.log("WS closed"); connectionState = 0;});
  ws.addEventListener("message", (data) => onMessage(data)); // <-- add this line
  for (let idx = 0; idx < driftPoolLength; idx++) {
	  driftPool[idx] = 0;
  }
  return ws;
};

// Send collected form data to the WebSocket server as JSON.
// Payload shape: { type: "formSubmit", modalId: "<id>", timestamp: <ms>, data: { id: value, … } }
const postFormData = (formData, mycommand, myscope) => {
  const socket = getWebSocket();
  const payload = JSON.stringify({
//    type: "formSubmit",
    _timestamp_: Date.now(),
    _command_:mycommand,
    _scope_:myscope,
    data: formData,
  });
 
  if (socket.readyState === WebSocket.OPEN) {
    socket.send(payload);
  } else {
    // Queue the send for when the connection is ready
    socket.addEventListener("open", () => socket.send(payload), { once: true });
  }
  console.log("Sent: " + payload);
};

function onMessage(event) {
  var state;
  
  console.log('Received');
  console.log(event.data);
  lastContact = Date.now();
  
  var myObj = JSON.parse(event.data);
  if (myObj['_timestamp_'] > 0){
    driftPool[driftPtr]  =  (myObj['_timestamp_']/1000 - lastContact);
	driftPtr++;
	if (driftPtr <= driftPoolLength) {
		driftPtr = 0;
	}
  }
  
  var command = myObj["_command_"];
  var keys = Object.keys(myObj["data"]);
  console.log("Command: " + command + " / " + lastContact + " // " + cumulatedDrift + " /-/ " + boxClockDrift + "( " + messagesReceived + ")");
  if (command == "_ping_") {
    const data = {};  
    data["pongdata"] = "some_data";
    postFormData(data, "_pong_"); 
  }
  if ( command == "c_visibility") {
    for (var i = 0; i < keys.length; i++){
      var key = keys[i];
      console.log("Visibility for key: " + key);
      if ( myObj["data"][key] == 'no') {
        document.getElementById(key).hidden = true;
      }else{
        document.getElementById(key).hidden = false;
      }  
    }
  }
  if ( command == "c_enordis") {
    for (var i = 0; i < keys.length; i++){
      var key = keys[i];
	  if ( myObj["data"][key] == 'disable') {
		console.log("Disabling key: " + key);
        document.getElementById(key).disabled = true;
      }else{
		console.log("Enabling key: " + key);
        document.getElementById(key).disabled = false;
      }  
    }
  }

  if ( command == "c_content") {
    for (var i = 0; i < keys.length; i++){
      var key = keys[i];
      console.log("Changing key: " + key);
      document.getElementById(key).innerHTML = "";
      document.getElementById(key).innerHTML = myObj["data"][key];

    }
  }
//    document.getElementById(key).value = myObj["data"][key];
  
}
 
