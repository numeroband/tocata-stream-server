const libopusdecoder = require('../static/opus-decoder');
const {TocataBroadcastConnection} = require('../static/tocata-broadcast-connection');
const WebSocket = require('ws');
const {WaveFile} = require('wavefile');
const fs = require('fs');

const MAX_SAMPLES = 10 * 48000

class TestBroadcast {
    constructor() {
        this.left = new Float32Array(MAX_SAMPLES);
        this.right = new Float32Array(MAX_SAMPLES);
        this.numSamples = 0;
    } 
    async run(uri, sessionId) {
        const {OpusDecoder} = await libopusdecoder();
        const connection = new TocataBroadcastConnection(new OpusDecoder(), this.onSamples.bind(this));
        connection.connect(WebSocket, uri, sessionId);    
    }

    onSamples({type, samples}) {
        const numSamples = samples[0].length;
        if (this.numSamples + numSamples > MAX_SAMPLES) {
            this.writeWav('/tmp/tocata_broadcast.wav');
            this.numSamples = 0;
        }
        console.log(`Copying ${numSamples} samples`);
        this.left.set(samples[0], this.numSamples);
        this.right.set(samples[1], this.numSamples);
        this.numSamples += numSamples;
    }

    writeWav(path) {
        console.log(`Writing ${path}`);
        const wav = new WaveFile();
        wav.fromScratch(2, 48000, '32f', [
            this.left,
            this.right
        ]);
        fs.writeFileSync(path, wav.toBuffer());
    }
}

const test = new TestBroadcast();
test.run(process.argv[2], process.argv[3]);