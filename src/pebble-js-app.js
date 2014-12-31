var DEBUG = true;


Pebble.addEventListener('ready',
  function(e) {
    console.log('JavaScript app ready and running!');
  }
);

Pebble.addEventListener("appmessage",
                        function(e) {
                          if (DEBUG) console.log("Pebble App Message!");
                          // Store 12/24hr setting passed from Pebble
                          if (e.payload !== undefined && e.payload.image_data !== undefined) {
                            if (DEBUG) {
                              console.log('Image data: ' + e.payload.image_data);
                              console.log('Image data byte 1: ' + e.payload.image_data[0]);
                              console.log('Image data byte 1 type: ' + typeof(e.payload.image_data[0]));
                              console.log('Image data chunk length: ' + e.payload.image_data.length);
                            }
                          }
                        });

Pebble.addEventListener("showConfiguration", 
                         function() {
                           if (DEBUG) console.log("Showing Settings...");
                           Pebble.openURL("data:text/html," + 
                                          encodeURIComponent('<html><body><h1>Hello World</h1><p><input type="button" value="Close" onClick="location.href=&quot;pebblejs://close#&quot;" /></p></body></html><!--.html'));
                          });


Pebble.addEventListener("webviewclosed",
                         function(e) {
                           if (DEBUG) console.log("Webview closed");
                           if (e.response) {
                             if (DEBUG) console.log("Settings returned: " + e.response);
                             
                           }
                           else {
                             if (DEBUG) console.log("Settings cancelled");
                           }
                         });