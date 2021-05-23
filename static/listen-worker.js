importScripts('opus-stream-decoder.js');

class Connection {
  constructor() {
    this.decoder = new OpusStreamDecoder({onDecode: this.onDecode.bind(this)});
    this.socket = null;
    this.flushSize = 24000;
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
      console.log('Received', event.data);
    } else {
      await this.decoder.ready;
      const array = new Uint8Array(await event.data.arrayBuffer());
      this.decoder.decode(array);
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

  onDecode({left, right, samplesDecoded, sampleRate}) {
    let processed = 0;
    while (processed < samplesDecoded) {
      const samplesToCopy = Math.min(samplesDecoded - processed, this.flushSize - this.numSamples);
      this.buffers[0].set(left.slice(processed, processed + samplesToCopy), this.numSamples);
      this.buffers[1].set(right.slice(processed, processed + samplesToCopy), this.numSamples);
      processed += samplesToCopy;
      this.numSamples += samplesToCopy;
      if (this.numSamples == this.flushSize) {
        postMessage({type: 'Samples', samples: this.buffers});
        this.numSamples = 0;        
      }
    }
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