const MAX_SOURCES = 4;

class AudioPlayer {
  constructor(audioCtx) {
    this.numSamples = 0;
    this.worker = new Worker("listen-worker.js");
    this.worker.onmessage = e => this.onMessage(e);
    this.audioCtx = audioCtx;
    this.nextSchedule = 0;
    this.pendingSources = [];
    this.playing = false;
  }

  play() {
    this.playing = true;
    this.nextSchedule = this.audioCtx.currentTime + (100 / 1000);
    this.schedule();
  }

  connect(url) {
    const protocol = url.protocol.replace("http", "ws");
    const sessionId = url.searchParams.get("sessionId");
    const wsUrl = `${protocol}//${url.host}`;

    this.worker.postMessage({
      type: 'Connect',
      url: wsUrl,
      sessionId,
    });    
  }

  onSamples(audio) {
    // Create an empty three-second stereo buffer at the sample rate of the AudioContext
    const numSamples = audio[0].length;
    const arrayBuffer = this.audioCtx.createBuffer(audio.length, numSamples, 48000);
    const bufferSource = this.audioCtx.createBufferSource();
    bufferSource.buffer = arrayBuffer;
    bufferSource.connect(this.audioCtx.destination);

    for (let channel = 0; channel < arrayBuffer.numberOfChannels; ++channel) {
      // This gives us the actual ArrayBuffer that contains the data
      const nowBuffering = arrayBuffer.getChannelData(channel);
      const samples = audio[channel];
      for (let i = 0; i < numSamples; ++i) {
        nowBuffering[i] = samples[i];
      }
    }
    this.pendingSources.push(bufferSource);
    if (!this.playing && this.pendingSources.length > MAX_SOURCES) {
      this.pendingSources.shift();
    }
    console.log(`queued ${numSamples} samples (queue size ${this.pendingSources.length})`);
  }

  schedule() {
    console.log(`scheduling ${this.pendingSources.length} starting ${this.nextSchedule}`);
    while (this.pendingSources.length > 0) {
      const source = this.pendingSources.shift();
      source.onended = () => this.schedule();
      source.start(this.nextSchedule);
      this.nextSchedule += source.buffer.duration;
    }
  }

  onMessage(e) {
    switch (e.data.type) {
      case 'Samples':
        this.onSamples(e.data.samples);
        break;
      default:
        break;
    }  
  }
}

const audioCtx = new (window.AudioContext || window.webkitAudioContext)({
  latencyHint: 'interactive',
  sampleRate: 48000,
});
const player = new AudioPlayer(audioCtx);
player.connect(new URL(window.location));
