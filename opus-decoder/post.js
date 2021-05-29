class OpusDecoder {
  constructor() {
    this.decoder = null;
  }

  initDecoder() {
    this.decoder = new Module.OpusDecoderPriv();
    this.buffer = this.decoder.buffer();
    this.leftSamples = this.decoder.leftSamples();
    this.rightSamples = this.decoder.rightSamples();
  }

  delete() {
    this.decoder.delete();
    this.decoder = null;
  }

  decode(frame) {
    return new Promise((resolve, reject) => {
      if (!this.decoder) {
        this.initDecoder();
      }

      Module.HEAPU8.set(frame, this.buffer);
      const samples = this.decoder.decode(frame.length);
      if (samples < 1) {
        reject("Cannot decode frame");
        return;
      }

      const leftSamples = new Float32Array(Module.HEAPF32.buffer, this.leftSamples, samples);
      const rightSamples = new Float32Array(Module.HEAPF32.buffer, this.rightSamples, samples);
      resolve({leftSamples, rightSamples});
    });
  }
}

Module['OpusDecoder'] = OpusDecoder
