var DEBUG = true;

var ChunkStatusEnum = {FIRST_CHUNK: 1, MID_CHUNK: 2, LAST_CHUNK: 3};

var image_data = [];
var chunk_status = 0;

function to4Byte(value) {
  return String.fromCharCode(value&0xff, (value>>8)&0xff, (value>>16)&0xff, (value>>24)&0xff);
}

function swapByteEndianness(val) { 
  return ((val & 0x1) << 7) | 
          ((val & 0x2) << 5) | 
          ((val & 0x4) << 3) | 
          ((val & 0x8) << 1) | 
          ((val >> 1) & 0x8) | 
          ((val >> 3) & 0x4) | 
          ((val >> 5) & 0x2) | 
          ((val >> 7) & 0x1); 
}

function CreateBMP(pixel_data, width, height) {
  var bmp;
 
  // BMP Header
  bmp = 'BM';                            // ID
  bmp += to4Byte(62 + pixel_data.length); // File size
  bmp += to4Byte(0);                      // Unused
  bmp += to4Byte(62);                     // Pixel offset
  
  // DIB Header
  bmp += to4Byte(40);                 // DIB header length
  bmp += to4Byte(width);
  bmp += to4Byte(height);
  bmp += String.fromCharCode(1, 0);   // colour panes
  bmp += String.fromCharCode(1, 0);   // 1 bit per pixel
  bmp += to4Byte(0);                  // No compression
  bmp += to4Byte(pixel_data.length);
  bmp += to4Byte(2835);               // Horizontal print resolution
  bmp += to4Byte(2835);               // Vertical print resolution
  bmp += to4Byte(0);                  // Color palette - don't care
  bmp += to4Byte(0);                  // All colors important
  
  // 1-bit B&W Palette
  bmp += to4Byte(0);
  bmp += to4Byte(255 + (255*256) + (255*65536));
  
  // Reverse vertical order of pixel rows
  var row_size = width / 8;
  while (row_size % 4 !== 0) row_size++;  // Row size is always buffered to be a multiple of 4 (bytes)
  
  var pixels = [];
  
  for (var y = height-1; y >= 0; y--) {
    for (var x = y * row_size; x < (y+1) * row_size; x++) {
      pixels.push(swapByteEndianness(pixel_data[x]));
    }
  }
  //for (var i = pixel_data.length - 1; i >= 0; i--) {
  //  pixels = pixels.concat(pixel_data[i]);
  //}
  
  // Pixel data
  bmp += String.fromCharCode.apply(String, pixels);
  
  return bmp;
}

Pebble.addEventListener('ready',
  function(e) {
    console.log('JavaScript app ready and running!');
    if (localStorage.imageData !== undefined) {
      image_data = localStorage.imageData;
    }
    if (localStorage.chunkStatus !== undefined) {
      chunk_status = localStorage.chunkStatus;
    }
  }
);

var Base64 = {

  // private property
  _keyStr : "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=",
  
  // public method for encoding
  encode : function (input) {
      var output = "";
      var chr1, chr2, chr3, enc1, enc2, enc3, enc4;
      var i = 0;
  
      while (i < input.length) {
  
          chr1 = input.charCodeAt(i++);
          chr2 = input.charCodeAt(i++);
          chr3 = input.charCodeAt(i++);
  
          enc1 = chr1 >> 2;
          enc2 = ((chr1 & 3) << 4) | (chr2 >> 4);
          enc3 = ((chr2 & 15) << 2) | (chr3 >> 6);
          enc4 = chr3 & 63;
  
          if (isNaN(chr2)) {
              enc3 = enc4 = 64;
          } else if (isNaN(chr3)) {
              enc4 = 64;
          }
  
          output = output +
          this._keyStr.charAt(enc1) + this._keyStr.charAt(enc2) +
          this._keyStr.charAt(enc3) + this._keyStr.charAt(enc4);
  
      }
  
      return output;
  }
};

Pebble.addEventListener("appmessage",
                        function(e) {
                          if (DEBUG) console.log("Pebble App Message!");
                          // Store 12/24hr setting passed from Pebble
                          if (e.payload !== undefined && e.payload.image_data !== undefined && 
                              e.payload.chunk_status !== undefined) {
                            if (DEBUG) {
                              console.log('Chunk status: ' + e.payload.chunk_status);
                              console.log('Image data: ' + e.payload.image_data);
                              console.log('Image data chunk length: ' + e.payload.image_data.length);
                            }
                            
                            chunk_status = e.payload.chunk_status;
                            
                            switch (e.payload.chunk_status) {
                              case ChunkStatusEnum.FIRST_CHUNK:
                                image_data = e.payload.image_data;
                                break;
                              case ChunkStatusEnum.LAST_CHUNK:
                                image_data = image_data.concat(e.payload.image_data);
                                localStorage.imageData = image_data;
                                localStorage.chunkStatus = chunk_status;
                                break;
                              default:
                                image_data = image_data.concat(e.payload.image_data);
                                break;
                            }
                          }
                        });

Pebble.addEventListener("showConfiguration", 
                         function() {
                           var bmp = CreateBMP(image_data, 144, 168);
                           
                           if (DEBUG) {
                             console.log("Showing Settings...");
                             console.log("Image data length: " + image_data.length);
                             console.log("BMP data length: " + bmp.length);
                             console.log("Base64 BMP: " + Base64.encode(bmp));
                           }
                           Pebble.openURL("data:text/html," + 
                                          encodeURIComponent('<html><head><meta name="viewport" content="width=device-width, initial-scale=1" /></head><body><h1>Your Drawing</h1><p>Hold down on the image to save or copy</p><p><img src="data:image/bmp;base64,' +
                                                             Base64.encode(bmp) + '" style="width:90%;" /></p><p><input type="button" value="Close" onClick="location.href=&quot;pebblejs://close#&quot;" /></p></body></html><!--.html'));
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