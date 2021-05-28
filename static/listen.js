const MAX_SOURCES = 8;

class AudioPlayer {
  constructor(audioCtx) {
    this.numSamples = 0;
    this.worker = new Worker("listen-worker.js");
    this.worker.onmessage = e => this.onMessage(e);
    this.audioCtx = audioCtx;
    this.nextSchedule = 0;
    this.firstSource = null;
    this.playing = false;
  }

  play() {
    console.log('Start playback');
    this.playing = true;
    this.audioCtx.resume();
    this.schedule(this.firstSource);
    this.firstSource = null;
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
    if (this.playing) {
      this.schedule(bufferSource);
    } else {
      this.firstSource = bufferSource;
    }
  }

  schedule(source) {
    if (!this.nextSchedule) {
      this.nextSchedule = this.audioCtx.currentTime + 1.1;
    }
    // console.log(`scheduling at ${this.audioCtx.currentTime} starting ${this.nextSchedule}`);
    source.start(this.nextSchedule);
    this.nextSchedule += source.buffer.duration;
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
  latencyHint: 'interactive'
});
const player = new AudioPlayer(audioCtx);
player.connect(new URL(window.location));
