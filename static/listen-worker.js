importScripts('opus-decoder.js');

class Connection {
  constructor() {
    this.socket = null;
    this.numSamples = 0;
    this.buffers = [new Float32Array(this.flushSize), new Float32Array(this.flushSize)]
  }

  connect(url, sessionId) {
    console.log(`connecting to ${url}`);
    this.sessionId = sessionId;
    this.socket = new WebSocket(url);
    this.socket.onopen = e => this.onOpen(e);
    this.socket.onmessage = e => this.onMessage(e);
    this.socket.onclose = e => this.onClose(e);    
    this.socket.onerror = error => console.error(`[error] ${error.message}`);
    console.log("Listening!!!!!!!!")
  }

  onOpen(event) {
    const msg = {
      type: "Listen",
      sessionId: this.sessionId,
    };
    this.socket.send(JSON.stringify(msg));
  }

  async onMessage(event) {
    if (typeof(event.data) === 'string') {
    } else {
      const array = new Uint8Array(await event.data.arrayBuffer());
      if (!this.decoder) {
        console.log('Creating decoder');
        this.decoder = new OpusDecoder();
      }
      this.decoder.decode(array, this.onDecode.bind(this));
    }  
  }

  onClose(event) {
    if (event.wasClean) {
      console.log(`[close] Connection closed cleanly, code=${event.code} reason=${event.reason}`);
    } else {
      // e.g. server process killed or network down
      // event.code is usually 1006 in this case
      console.error('[close] Connection died');
    }  
    this.socket = null;
  }

  onDecode(left, right) {
    // console.log('samples decoded', left.length, right.length);
    postMessage({type: 'Samples', samples: [left, right]});
  }
}

const connection = new Connection();
onmessage = e => {
  switch (e.data.type) {
    case 'Connect':
      connection.connect(e.data.url, e.data.sessionId);
      break;
    default:
      break;
  }
}
console.log('Web Worker running');