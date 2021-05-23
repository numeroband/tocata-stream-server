importScripts('opus-decoder.js');
importScripts('tocata-broadcast-connection.js');

let connection = null;
onmessage = async e => {
  switch (e.data.type) {
    case 'Connect':
      if (!connection) {
        const {OpusDecoder} = await libopusdecoder();
        connection = new TocataBroadcastConnection(new OpusDecoder(), e => postMessage(e));
      }
      connection.connect(WebSocket, e.data.url, e.data.sessionId);
      break;
    default:
      break;
  }
}
console.log('Web Worker running');