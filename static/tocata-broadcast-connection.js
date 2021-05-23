class TocataBroadcastConnection {
  constructor(decoder, callback) {
    this.socket = null;
    this.decoder = decoder;
    this.callback = callback
  }

  async connect(webSocketClass, url, sessionId) {
    console.log(`connecting to ${url}`);
    this.sessionId = sessionId;
    this.socket = new webSocketClass(url);
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
      const array = event.data.arrayBuffer ? new Uint8Array(await event.data.arrayBuffer()) : event.data;
      if (!this.decoder) {
        console.log('Creating decoder');
        this.decoder = new OpusDecoder();
      }
      const {leftSamples, rightSamples} = await this.decoder.decode(array);
      this.callback({type: 'Samples', samples: [leftSamples, rightSamples]});
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
}

try {
  console.log('exporting module');
  module.exports = {TocataBroadcastConnection}
} catch(e) {}
