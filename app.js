(function() {
  var M = null;
  var FF = null;
  var ffmpegReady = false;

  var $wasmStatus = document.getElementById('wasm-status');
  var $ffmpegStatus = document.getElementById('ffmpeg-status');
  var $selfcheckStatus = document.getElementById('selfcheck-status');
  var $logBody = document.getElementById('log-body');
  var $logPanel = document.getElementById('log-panel');
  var $toggleLog = document.getElementById('toggle-log');
  var $clearLog = document.getElementById('clear-log');

  function setPill(el, text, cls) {
    el.textContent = text;
    el.className = 'pill ' + cls;
  }

  function pad2(n) { return n < 10 ? '0' + n : '' + n; }

  function logTime() {
    var d = new Date();
    return pad2(d.getHours()) + ':' + pad2(d.getMinutes()) + ':' + pad2(d.getSeconds()) + '.' + String(d.getMilliseconds()).padStart(3,'0');
  }

  var Logger = {
    _add: function(msg, cls) {
      var e = document.createElement('div');
      e.className = 'log-entry ' + cls;
      var t = document.createElement('span'); t.className = 'log-time'; t.textContent = logTime();
      var m = document.createElement('span'); m.className = 'log-msg'; m.textContent = msg;
      e.appendChild(t); e.appendChild(m);
      $logBody.appendChild(e);
      $logBody.scrollTop = $logBody.scrollHeight;
    },
    info: function(m) { this._add(m, 'info'); },
    ok: function(m)   { this._add(m, 'ok'); },
    warn: function(m) { this._add(m, 'warn'); },
    err: function(m)  { this._add(m, 'err'); },
    log: function(m)  { this._add(m, ''); },
    clear: function() { $logBody.innerHTML = ''; }
  };

  $toggleLog.addEventListener('click', function() {
    $logPanel.classList.toggle('hidden');
  });
  $clearLog.addEventListener('click', function() { Logger.clear(); });

  var favicon = (function() {
    var canvas = document.createElement('canvas'); canvas.width = 64; canvas.height = 64;
    var ctx = canvas.getContext('2d');
    var link = document.createElement('link'); link.rel = 'icon'; link.type = 'image/png';
    document.head.appendChild(link);
    var blinkTimer = null;
    function set(emoji, color) {
      if (blinkTimer) { clearInterval(blinkTimer); blinkTimer = null; }
      ctx.clearRect(0, 0, 64, 64);
      ctx.fillStyle = color || getComputedStyle(document.body).color || '#e0e0e0';
      ctx.font = '48px system-ui, sans-serif'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
      ctx.fillText(emoji, 32, 34);
      link.href = canvas.toDataURL('image/png');
    }
    function blink(emoji, ms) {
      var visible = true;
      blinkTimer = setInterval(function() {
        visible = !visible;
        if (visible) set(emoji); else { ctx.clearRect(0, 0, 64, 64); link.href = canvas.toDataURL('image/png'); }
      }, 300);
      setTimeout(function() { clearInterval(blinkTimer); blinkTimer = null; set(':)'); }, ms);
    }
    set('⏳', '#fbbf24');
    return { set: set, blink: blink };
  })();

  function showError(msg) {
    Logger.err(msg); favicon.blink(':(', 5000);
    showModal('modal-error', msg);
  }
  function showSuccess(msg) {
    Logger.ok(msg); showModal('modal-success', msg);
  }
  function showModal(id, msg) {
    var d = document.getElementById(id);
    if (id === 'modal-error') document.getElementById('modal-error-msg').textContent = msg;
    if (id === 'modal-success') document.getElementById('modal-success-msg').textContent = msg;
    d.showModal();
  }

  function updateFfmpegLoad(text, pct) {
    document.getElementById('ffmpeg-load-msg').textContent = text;
    var bar = document.getElementById('ffmpeg-load-bar');
    if (pct >= 0) bar.value = pct;
  }

  function fmtSize(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / 1048576).toFixed(2) + ' MB';
  }

  function downloadBlob(data, filename) {
    var blob = new Blob([data], {type: 'application/octet-stream'});
    var url = URL.createObjectURL(blob);
    var a = document.createElement('a'); a.href = url; a.download = filename;
    document.body.appendChild(a); a.click(); document.body.removeChild(a);
    setTimeout(function() { URL.revokeObjectURL(url); }, 5000);
  }

  function readFile(file) {
    return new Promise(function(resolve, reject) {
      var reader = new FileReader();
      reader.onload = function() { resolve(new Uint8Array(reader.result)); };
      reader.onerror = function() { reject(reader.error); };
      reader.readAsArrayBuffer(file);
    });
  }

  function resultBlock(title, meta, data, filename) {
    var div = document.createElement('div'); div.className = 'result-block';
    var h = document.createElement('div'); h.className = 'result-title'; h.textContent = title;
    div.appendChild(h);
    if (meta) { var m = document.createElement('div'); m.className = 'result-meta'; m.textContent = meta; div.appendChild(m); }
    if (data && filename) {
      var act = document.createElement('div'); act.className = 'result-actions';
      var btn = document.createElement('button'); btn.textContent = 'Download ' + filename;
      btn.addEventListener('click', function() { downloadBlob(data, filename); });
      act.appendChild(btn); div.appendChild(act);
    }
    return div;
  }

  function makeTable(rows) {
    var table = document.createElement('table');
    rows.forEach(function(r) {
      var tr = document.createElement('tr');
      var th = document.createElement('th'); th.textContent = r[0];
      var td = document.createElement('td'); td.textContent = r[1];
      tr.appendChild(th); tr.appendChild(td); table.appendChild(tr);
    });
    return table;
  }

  var WASM = {
    open: function(ptr,sz){ return M.ccall('wasm_pmf_open','number',['number','number'],[ptr,sz]); },
    demux: function(v,a){ return M.ccall('wasm_pmf_demux','number',['number','number'],[v,a]); },
    videoPtr: function(){ return M.ccall('wasm_pmf_get_video_ptr','number',[],[]); },
    videoSize: function(){ return M.ccall('wasm_pmf_get_video_size','number',[],[]); },
    audioPtr: function(i){ return M.ccall('wasm_pmf_get_audio_ptr','number',['number'],[i]); },
    audioSize: function(i){ return M.ccall('wasm_pmf_get_audio_size','number',['number'],[i]); },
    audioCount: function(){ return M.ccall('wasm_pmf_get_audio_count','number',[],[]); },
    close: function(){ M.ccall('wasm_pmf_close',null,[],[]); },
    isMps: function(ptr,sz){ return M.ccall('wasm_pmf_is_mps','number',['number','number'],[ptr,sz]); },
    hasAudio: function(ptr,sz){ return M.ccall('wasm_pmf_has_audio','number',['number','number'],[ptr,sz]); },
    h264Frames: function(ptr,sz){ return M.ccall('wasm_pmf_count_h264_frames','number',['number','number'],[ptr,sz]); },
    syncFrames: function(ptr,sz){ return M.ccall('wasm_pmf_count_sync_frames','number',['number','number'],[ptr,sz]); },
    mps2pmf: function(ptr,sz,mins,secs,icon,vonly,outSizePtr){
      return M.ccall('wasm_pmf_mps_to_pmf','number',['number','number','number','number','number','number','number'],[ptr,sz,mins,secs,icon,vonly,outSizePtr]);
    },
    at3EncodePcm: function(pcmPtr,samples,sr,ch,br){
      return M.ccall('wasm_at3_encode_pcm','number',['number','number','number','number','number'],[pcmPtr,samples,sr,ch,br]);
    },
    at3EncodedPtr: function(){ return M.ccall('wasm_at3_get_encoded_ptr','number',[],[]); },
    at3EncodedSize: function(){ return M.ccall('wasm_at3_get_encoded_size','number',[],[]); },
    at3Decode: function(ptr,sz){
      return M.ccall('wasm_at3_decode','number',['number','number'],[ptr,sz]);
    },
    at3DecodedPtr: function(){ return M.ccall('wasm_at3_get_decoded_ptr','number',[],[]); },
    at3DecodedSamples: function(){ return M.ccall('wasm_at3_get_decoded_samples','number',[],[]); },
    alloc: function(data){ var p=M._malloc(data.length); M.HEAPU8.set(data,p); return p; },
    read: function(ptr,size){ return new Uint8Array(M.HEAPU8.buffer,ptr,size).slice(); },
    readFloat: function(ptr,count){ return new Float32Array(M.HEAPU8.buffer,ptr,count).slice(); }
  };

  function setupDragDrop() {
    document.querySelectorAll('.drop-zone').forEach(function(zone) {
      var fileId = zone.dataset.drop;
      var input = document.getElementById(fileId);
      if (!input) return;
      zone.addEventListener('dragover', function(e) { e.preventDefault(); zone.style.borderColor = 'var(--accent)'; });
      zone.addEventListener('dragleave', function() { zone.style.borderColor = ''; });
      zone.addEventListener('drop', function(e) {
        e.preventDefault(); zone.style.borderColor = '';
        if (e.dataTransfer.files.length > 0) {
          input.files = e.dataTransfer.files;
          input.dispatchEvent(new Event('change', {bubbles: true}));
        }
      });
      input.addEventListener('change', function() {
        var label = zone.querySelector('.drop-label span:last-child');
        if (label && input.files.length > 0)
          label.textContent = input.files[0].name + ' (' + fmtSize(input.files[0].size) + ')';
      });
    });
  }

  function setupDemux() {
    var form = document.getElementById('demux-form');
    var out = document.getElementById('demux-output');
    var fileEl = document.getElementById('demux-file');

    form.addEventListener('submit', function(e) {
      e.preventDefault(); out.innerHTML = '';
      var f = fileEl.files[0];
      if (!f) { showError('No file selected'); return; }

      Logger.info('demux: reading ' + f.name + ' (' + fmtSize(f.size) + ')');
      readFile(f).then(function(data) {
        var ptr = WASM.alloc(data);
        if (!WASM.open(ptr, data.length)) { M._free(ptr); throw new Error('not a valid PMF file'); }
        M._free(ptr);
        Logger.info('demux: PMF opened');

        var v = form.querySelector('[data-out="video"]').checked ? 1 : 0;
        var a = form.querySelector('[data-out="audio"]').checked ? 1 : 0;
        var split = document.getElementById('demux-split').checked;
        var addHdr = document.getElementById('demux-header').checked;

        Logger.info('demux: video=' + v + ' audio=' + a + ' split=' + split + ' header=' + addHdr);
        if (!WASM.demux(v, a)) { WASM.close(); throw new Error('demux failed'); }
        Logger.info('demux: streams extracted');

        if (v) {
          var vsz = WASM.videoSize();
          if (vsz > 0) {
            var vd = WASM.read(WASM.videoPtr(), vsz);
            out.appendChild(resultBlock('Video Stream (.264)', fmtSize(vsz), vd, 'video.264'));
            Logger.ok('video: ' + fmtSize(vsz));
          } else { Logger.warn('no video stream found'); }
        }
        if (a) {
          var ac = WASM.audioCount();
          Logger.info('audio streams: ' + ac);
          for (var i = 0; i < ac; i++) {
            var asz = WASM.audioSize(i);
            if (asz > 0) {
              var ad = WASM.read(WASM.audioPtr(i), asz);
              var an = ac > 1 ? 'audio.' + i + '.at3' : 'audio.at3';
              if (addHdr) {
                Logger.warn('OMA header wrapping not available in browser WASM — use native CLI');
              }
              out.appendChild(resultBlock('Audio Stream ' + i + ' (.at3)', fmtSize(asz), ad, an));
              Logger.ok('audio[' + i + ']: ' + fmtSize(asz));
            }
          }
        }
        WASM.close();
        showSuccess('Demux complete');
      }).catch(function(err) { showError(err.message); try { WASM.close(); } catch(_) {} });
    });
  }

  function setupMps2Pmf() {
    var form = document.getElementById('mps2pmf-form');
    var out = document.getElementById('mps2pmf-output');
    var fileEl = document.getElementById('mps2pmf-file');

    form.addEventListener('submit', function(e) {
      e.preventDefault(); out.innerHTML = '';
      var f = fileEl.files[0];
      if (!f) { showError('No file selected'); return; }

      Logger.info('mps2pmf: reading ' + f.name + ' (' + fmtSize(f.size) + ')');
      readFile(f).then(function(data) {
        var ptr = WASM.alloc(data);
        if (!WASM.isMps(ptr, data.length)) { M._free(ptr); throw new Error('not a valid MPS file'); }

        var mins = parseInt(document.getElementById('mps-mins').value) || 0;
        var secs = parseInt(document.getElementById('mps-secs').value) || 0;
        var icon = document.getElementById('mps-icon').checked ? 1 : 0;
        var audioProbe = document.getElementById('mps-audio-probe').checked;
        var vOnly = 0;

        if (audioProbe) {
          var ah = WASM.hasAudio(ptr, data.length);
          vOnly = ah ? 0 : 1;
          Logger.info('audio probe: ' + (ah ? 'found' : 'none, using video-only'));
        } else {
          vOnly = document.getElementById('mps-video-only').checked ? 1 : 0;
        }
        Logger.info('settings: icon=' + icon + ' video-only=' + vOnly + ' dur=' + mins + ':' + secs);

        var outSizePtr = M._malloc(4);
        var resultPtr = WASM.mps2pmf(ptr, data.length, mins, secs, icon, vOnly, outSizePtr);
        var outSize = M.HEAPU32[outSizePtr >> 2];
        M._free(ptr);

        if (!resultPtr || outSize === 0) { M._free(outSizePtr); throw new Error('conversion failed'); }

        var outData = WASM.read(resultPtr, outSize);
        M._free(outSizePtr);

        var oname = f.name.replace(/\.mps$/i, '') + '.pmf';
        out.appendChild(resultBlock('PMF Output', fmtSize(outSize), outData, oname));
        Logger.ok('pmf: ' + fmtSize(outSize));
        showSuccess('PMF created: ' + fmtSize(outSize));
      }).catch(function(err) { showError(err.message); });
    });
  }

  function setupAt3() {
    var form = document.getElementById('at3-form');
    var out = document.getElementById('at3-output');
    var fileEl = document.getElementById('at3-file');
    var submitBtn = form.querySelector('button[type="submit"]');

    fileEl.addEventListener('change', function() {
      if (fileEl.files.length > 0) {
        submitBtn.textContent = fileEl.files[0].name.toLowerCase().endsWith('.wav') ? 'Encode to AT3' : 'Analyze';
      } else {
        submitBtn.textContent = 'Analyze';
      }
    });

    var FRAME_SIZES = [
      {br: 32, sz: 96}, {br: 48, sz: 168}, {br: 66, sz: 192},
      {br: 96, sz: 288}, {br: 128, sz: 384}, {br: 160, sz: 480},
      {br: 192, sz: 576}, {br: 256, sz: 768}, {br: 320, sz: 960}
    ];

    form.addEventListener('submit', function(e) {
      e.preventDefault(); out.innerHTML = '';
      var f = fileEl.files[0];
      if (!f) { showError('No file selected'); return; }

      var isWav = f.name.toLowerCase().endsWith('.wav');

      if (isWav) {
        handleWavEncode(f, out);
      } else {
        handleAt3Analyze(f, out, FRAME_SIZES);
      }
    });
  }

  function handleAt3Analyze(f, out, FRAME_SIZES) {
    Logger.info('at3: analyzing ' + f.name + ' (' + fmtSize(f.size) + ')');
    readFile(f).then(function(data) {
      var sz = data.length;
      var isOma = sz > 4 && data[0]===0x45 && data[1]===0x41 && data[2]===0x33 && data[3]===0x03;
      var isAa3 = sz > 4 && data[0]===0x65 && data[1]===0x61 && data[2]===0x33 && data[3]===0x03;
      var hasHeader = isOma || isAa3;
      var payloadSz = sz;

      var rows = [['File size', fmtSize(sz)]];
      if (hasHeader) {
        rows.push(['Container', isOma ? 'OMA (.oma)' : 'AA3 (.aa3)']);
        payloadSz = sz - 0x460;
        if (payloadSz < 0) payloadSz = 0;
        rows.push(['Header size', fmtSize(sz - payloadSz)]);
      }
      rows.push(['Payload', fmtSize(payloadSz)]);

      var matches = [];
      FRAME_SIZES.forEach(function(fs) {
        if (payloadSz > 0 && payloadSz % fs.sz === 0) {
          var frames = payloadSz / fs.sz;
          var dur = frames * 1024 / 44100;
          matches.push({br: fs.br, sz: fs.sz, frames: frames, dur: dur});
        }
      });

      if (matches.length > 0) {
        rows.push(['Bitrate matches', '']);
        matches.forEach(function(m) {
          rows.push(['  ' + m.br + ' kbps', m.frames + ' frames, ~' + m.dur.toFixed(1) + 's @ ' + m.sz + ' B/frame']);
        });
      } else {
        rows.push(['Bitrate', 'unknown']);
        var estFrames = Math.floor(payloadSz / 192);
        if (estFrames > 0) rows.push(['Est frames', '~' + estFrames + ' (@ 66 kbps)']);
      }

      var block = document.createElement('div'); block.className = 'result-block';
      var title = document.createElement('div'); title.className = 'result-title'; title.textContent = f.name;
      block.appendChild(title); block.appendChild(makeTable(rows));

      var act = document.createElement('div'); act.className = 'result-actions';
      var btnDec = document.createElement('button'); btnDec.textContent = 'Decode to WAV';
      btnDec.addEventListener('click', function() { decodeAt3ToWav(data, f.name); });
      act.appendChild(btnDec);
      block.appendChild(act);

      out.appendChild(block);
      Logger.ok('at3 analysis complete');
    }).catch(function(err) { showError(err.message); });
  }

  function handleWavEncode(f, out) {
    Logger.info('at3: encoding WAV ' + f.name + ' (' + fmtSize(f.size) + ')');
    readFile(f).then(function(data) {
      if (data.length < 44 || String.fromCharCode(data[0],data[1],data[2],data[3]) !== 'RIFF') {
        throw new Error('not a valid WAV file');
      }

      var channels = data[22] | (data[23] << 8);
      var sampleRate = data[24] | (data[25] << 8) | (data[26] << 16) | (data[27] << 24);
      var bitsPerSample = data[34] | (data[35] << 8);
      var dataOffset = 44;
      for (var i = 36; i < data.length - 4; i++) {
        if (data[i]===0x64 && data[i+1]===0x61 && data[i+2]===0x74 && data[i+3]===0x61) {
          dataOffset = i + 8; break;
        }
      }

      Logger.info('WAV: ' + channels + 'ch ' + sampleRate + 'Hz ' + bitsPerSample + 'bit');
      var pcmLen = data.length - dataOffset;
      var sampleCount = bitsPerSample === 16 ? pcmLen / 2 / channels : pcmLen / channels;
      var floats = new Float32Array(sampleCount * channels);

      if (bitsPerSample === 16) {
        var i16 = new Int16Array(data.buffer, data.byteOffset + dataOffset, pcmLen / 2);
        for (var i = 0; i < sampleCount * channels; i++) {
          floats[i] = i16[i] / 32768.0;
        }
      } else if (bitsPerSample === 8) {
        for (var i = 0; i < sampleCount * channels; i++) {
          floats[i] = (data[dataOffset + i] - 128) / 128.0;
        }
      } else {
        throw new Error('unsupported bit depth: ' + bitsPerSample);
      }

      Logger.info('pcm: ' + sampleCount + ' samples, ' + channels + ' channels');

      var floatPtr = M._malloc(floats.length * 4);
      new Float32Array(M.HEAPU8.buffer).set(floats, floatPtr >> 2);

      var br = 128;
      var resultSz = WASM.at3EncodePcm(floatPtr, sampleCount, sampleRate, channels, br);
      M._free(floatPtr);

      if (resultSz <= 0) throw new Error('AT3 encoding failed (returned ' + resultSz + ')');

      var at3Data = WASM.read(WASM.at3EncodedPtr(), resultSz);
      Logger.ok('AT3 encoded: ' + fmtSize(resultSz) + ' @ ' + br + ' kbps');

      var block = document.createElement('div'); block.className = 'result-block';
      var title = document.createElement('div'); title.className = 'result-title'; title.textContent = f.name;
      block.appendChild(title);
      block.appendChild(makeTable([
        ['Source', f.name + ' (' + fmtSize(data.length) + ')'],
        ['Format', channels + 'ch ' + sampleRate + 'Hz ' + bitsPerSample + 'bit'],
        ['Samples', String(sampleCount)],
        ['Encoded', fmtSize(resultSz) + ' @ ' + br + ' kbps ATRAC3'],
        ['Ratio', (resultSz / data.length * 100).toFixed(1) + '% of original']
      ]));

      var act = document.createElement('div'); act.className = 'result-actions';
      var btnDl = document.createElement('button'); btnDl.textContent = 'Download .at3';
      var outName = f.name.replace(/\.wav$/i, '') + '.at3';
      btnDl.addEventListener('click', function() { downloadBlob(at3Data, outName); });
      act.appendChild(btnDl);
      block.appendChild(act);

      out.appendChild(block);
      showSuccess('AT3 encoded: ' + fmtSize(resultSz));
    }).catch(function(err) { showError(err.message); });
  }

  function decodeAt3ToWav(at3Data, filename) {
    Logger.info('at3: decoding ' + filename + ' (' + fmtSize(at3Data.length) + ')');
    try {
      var ptr = WASM.alloc(at3Data);
      var decoded = WASM.at3Decode(ptr, at3Data.length);
      M._free(ptr);

      if (decoded <= 0) throw new Error('AT3 decode failed (returned ' + decoded + ')');

      var samples = WASM.at3DecodedSamples();
      var pcm = WASM.readFloat(WASM.at3DecodedPtr(), samples * 2);
      Logger.ok('decoded: ' + samples + ' samples, ' + pcm.length + ' floats');

      var wavBuf = buildWav(pcm, 44100, 2);
      var outName = filename.replace(/\.(at3|oma|aa3)$/i, '') + '.wav';

      var block = document.createElement('div'); block.className = 'result-block';
      var title = document.createElement('div'); title.className = 'result-title'; title.textContent = 'Decoded: ' + filename;
      block.appendChild(title);
      block.appendChild(makeTable([
        ['Samples', String(samples)],
        ['Duration', (samples / 44100).toFixed(1) + 's'],
        ['Output', fmtSize(wavBuf.length)]
      ]));

      var act = document.createElement('div'); act.className = 'result-actions';
      var btnDl = document.createElement('button'); btnDl.textContent = 'Download .wav';
      btnDl.addEventListener('click', function() { downloadBlob(wavBuf, outName); });
      act.appendChild(btnDl);
      block.appendChild(act);

      var at3Out = document.getElementById('at3-output');
      at3Out.appendChild(block);
      showSuccess('Decoded: ' + samples + ' samples');
    } catch(err) {
      showError('Decode failed: ' + err.message);
    }
  }

  function buildWav(pcmFloats, sampleRate, channels) {
    var bytesPerSample = 2;
    var dataLen = pcmFloats.length * bytesPerSample;
    var buf = new ArrayBuffer(44 + dataLen);
    var view = new DataView(buf);

    function w32(o,v) { view.setUint32(o,v,true); }
    function w16(o,v) { view.setUint16(o,v,true); }

    w32(0, 0x46464952); w32(4, 36 + dataLen); w32(8, 0x45564157);
    w32(12, 0x20746D66); w32(16, 16); w16(20, 1); w16(22, channels);
    w32(24, sampleRate); w32(28, sampleRate * channels * bytesPerSample);
    w16(32, channels * bytesPerSample); w16(34, bytesPerSample * 8);
    w32(36, 0x61746164); w32(40, dataLen);

    var pcmView = new DataView(buf, 44);
    for (var i = 0; i < pcmFloats.length; i++) {
      var s = Math.max(-1, Math.min(1, pcmFloats[i]));
      var v = s < 0 ? s * 32768 : s * 32767;
      pcmView.setInt16(i * 2, v | 0, true);
    }
    return new Uint8Array(buf);
  }

  function setupInfo() {
    var form = document.getElementById('info-form');
    var out = document.getElementById('info-output');
    var fileEl = document.getElementById('info-file');

    form.addEventListener('submit', function(e) {
      e.preventDefault(); out.innerHTML = '';
      var f = fileEl.files[0];
      if (!f) { showError('No file selected'); return; }

      Logger.info('info: reading ' + f.name + ' (' + fmtSize(f.size) + ')');
      readFile(f).then(function(data) {
        var ptr = WASM.alloc(data);
        if (!WASM.open(ptr, data.length)) { M._free(ptr); throw new Error('not a valid PMF file'); }
        M._free(ptr);
        WASM.demux(1, 1);

        var vsz = WASM.videoSize();
        var ac = WASM.audioCount();

        var rows = [
          ['File size', fmtSize(data.length)],
          ['PMF magic', (data[0]===0x50&&data[1]===0x53&&data[2]===0x4D&&data[3]===0x46) ? 'PSMF (valid)' : 'INVALID'],
          ['Video stream', vsz > 0 ? fmtSize(vsz) + ' H.264' : 'none'],
          ['Audio streams', String(ac)]
        ];
        for (var i = 0; i < ac; i++) rows.push(['Audio ' + i, fmtSize(WASM.audioSize(i)) + ' ATRAC3']);

        var block = document.createElement('div'); block.className = 'result-block';
        var title = document.createElement('div'); title.className = 'result-title'; title.textContent = f.name;
        block.appendChild(title); block.appendChild(makeTable(rows));
        out.appendChild(block);

        Logger.ok('analysis complete');
        WASM.close();
      }).catch(function(err) { showError(err.message); try { WASM.close(); } catch(_) {} });
    });
  }

  function setupH264() {
    var form = document.getElementById('h264-form');
    var out = document.getElementById('h264-output');
    var fileEl = document.getElementById('h264-file');

    form.addEventListener('submit', function(e) {
      e.preventDefault(); out.innerHTML = '';
      var f = fileEl.files[0];
      if (!f) { showError('No file selected'); return; }

      Logger.info('h264: reading ' + f.name + ' (' + fmtSize(f.size) + ')');
      readFile(f).then(function(data) {
        var ptr = WASM.alloc(data);
        var frames = WASM.h264Frames(ptr, data.length);
        var syncs = WASM.syncFrames(ptr, data.length);
        M._free(ptr);

        Logger.log('frames: ' + frames + ' total, ' + syncs + ' sync');

        var block = document.createElement('div'); block.className = 'result-block';
        var title = document.createElement('div'); title.className = 'result-title'; title.textContent = f.name;
        block.appendChild(title);
        block.appendChild(makeTable([
          ['File size', fmtSize(data.length)],
          ['Total frames', String(frames)],
          ['Sync (IDR) frames', String(syncs)],
          ['Non-sync frames', String(frames - syncs)],
          ['GOP avg', syncs > 0 ? String(Math.round(frames / syncs)) : 'N/A']
        ]));
        out.appendChild(block);
      }).catch(function(err) { showError(err.message); });
    });
  }

  function setupTranscode() {
    var form = document.getElementById('transcode-form');
    var out = document.getElementById('transcode-output');
    var videoEl = document.getElementById('transcode-video');
    var audioEl = document.getElementById('transcode-audio');
    var submitBtn = document.getElementById('tc-submit');

    form.addEventListener('submit', function(e) {
      e.preventDefault();
      if (!ffmpegReady) { showError('ffmpeg.wasm is still loading — wait for the green indicator'); return; }

      out.innerHTML = '';
      var vf = videoEl.files[0];
      if (!vf) { showError('No video file selected'); return; }

      submitBtn.disabled = true; submitBtn.textContent = 'Encoding ...';

      var opts = {
        vbr: parseInt(document.getElementById('tc-vbitrate').value) || 1000,
        mbr: parseInt(document.getElementById('tc-maxbitrate').value) || 2000,
        w: parseInt(document.getElementById('tc-width').value) || 480,
        h: parseInt(document.getElementById('tc-height').value) || 272,
        gop: parseInt(document.getElementById('tc-gop').value) || 15,
        fps: parseInt(document.getElementById('tc-fps').value) || 30,
        crf: document.getElementById('tc-crf').value,
        preset: document.getElementById('tc-preset').value,
        profile: document.getElementById('tc-profile').value,
        icon: document.getElementById('tc-icon').checked,
        mins: parseInt(document.getElementById('tc-mins').value) || 0,
        secs: parseInt(document.getElementById('tc-secs').value) || 0,
        audioFile: audioEl.files[0]
      };

      Logger.info('transcode: ' + vf.name + ' (' + fmtSize(vf.size) + ')');
      Logger.info('settings: ' + opts.w + 'x' + opts.h + ' ' + opts.fps + 'fps @ ' + opts.vbr + '/' + opts.mbr + ' kbps');
      Logger.info('codec: crf=' + opts.crf + ' preset=' + opts.preset + ' profile=' + opts.profile + ' gop=' + opts.gop);
      Logger.info('pmf: icon=' + opts.icon + ' dur=' + opts.mins + ':' + opts.secs);
      if (opts.audioFile) Logger.info('audio: ' + opts.audioFile.name + ' (' + fmtSize(opts.audioFile.size) + ')');

      runTranscode(vf, opts).then(function(result) {
        var sz = result.length || result.byteLength || 0;
        out.appendChild(resultBlock('PMF Output', fmtSize(sz), result, vf.name.replace(/\.[^.]+$/, '') + '.pmf'));
        showSuccess('PMF created: ' + fmtSize(sz));
        submitBtn.disabled = false; submitBtn.textContent = 'Convert to PMF';
      }).catch(function(err) {
        showError(err.message);
        submitBtn.disabled = false; submitBtn.textContent = 'Convert to PMF';
      });
    });
  }

  var runTranscode = async function(videoFile, opts) {
    Logger.info('ffmpeg: writing input');
    var vdata = await readFile(videoFile);
    var inName = 'input' + (videoFile.name.match(/\.[^.]+$/) || ['.bin'])[0];
    await FF.writeFile(inName, vdata);

    var audioInName = null;
    if (opts.audioFile) {
      var adata = await readFile(opts.audioFile);
      audioInName = 'audio_in' + (opts.audioFile.name.match(/\.[^.]+$/) || ['.wav'])[0];
      await FF.writeFile(audioInName, adata);
      Logger.info('ffmpeg: wrote audio ' + audioInName + ' (' + fmtSize(adata.length) + ')');
    }

    var fpsFrac = opts.fps === 30 ? '30000/1001' : String(opts.fps);
    var scaleFilter = 'scale=' + opts.w + ':' + opts.h + ':force_original_aspect_ratio=decrease,pad=' + opts.w + ':' + opts.h + ':(ow-iw)/2:(oh-ih)/2,fps=' + fpsFrac;
    var x264opts = 'nal-hrd=none:profile=' + opts.profile + ':level=3.0:vbv-maxrate=' + opts.mbr + ':vbv-bufsize=' + (opts.mbr*2);

    var args = [
      '-i', inName,
      '-vcodec', 'libx264',
      '-preset', opts.preset,
      '-crf', opts.crf,
      '-b:v', opts.vbr + 'k',
      '-maxrate', opts.mbr + 'k',
      '-bufsize', (opts.mbr*2) + 'k',
      '-g', String(opts.gop),
      '-bf', '0', '-refs', '1',
      '-x264-params', x264opts,
      '-vf', scaleFilter,
      '-an', '-f', 'h264', 'video.264'
    ];

    Logger.info('ffmpeg: ' + args.join(' '));
    var t0 = Date.now();
    await FF.exec(args);
    Logger.ok('ffmpeg: done in ' + ((Date.now() - t0) / 1000).toFixed(1) + 's');

    var h264Data;
    try { h264Data = await FF.readFile('video.264'); } catch(_) { throw new Error('ffmpeg did not produce H.264 output'); }
    Logger.info('h264 stream: ' + fmtSize(h264Data.length));

    if (audioInName) {
      Logger.warn('ATRAC3 encoding not available in browser WASM build');
      Logger.warn('audio muxing skipped — use native videotopmf CLI for audio+video PMF');
    }

    Logger.info('wrapping H.264 into PMF');
    var h264U8 = new Uint8Array(h264Data);
    var ptrH264 = WASM.alloc(h264U8);

    var outSizePtr = M._malloc(4);
    var pmfPtr = WASM.mps2pmf(ptrH264, h264U8.length, opts.mins, opts.secs, opts.icon ? 1 : 0, 1, outSizePtr);
    var pmfSize = M.HEAPU32[outSizePtr >> 2];

    if (!pmfPtr || pmfSize === 0) { M._free(ptrH264); M._free(outSizePtr); throw new Error('PMF wrapping failed'); }

    var pmfData = WASM.read(pmfPtr, pmfSize);
    M._free(ptrH264); M._free(outSizePtr);

    Logger.ok('pmf done: ' + fmtSize(pmfData.length) + ' (' + ((Date.now() - t0) / 1000).toFixed(1) + 's total)');

    try { await FF.deleteFile(inName); } catch(_) {}
    try { await FF.deleteFile('video.264'); } catch(_) {}
    if (audioInName) try { await FF.deleteFile(audioInName); } catch(_) {}

    return pmfData;
  };

  var initFfmpeg = async function() {
    try {
      var { FFmpeg } = FFmpegWASM;
      FF = new FFmpeg();
      var loadModal = document.getElementById('modal-ffmpeg-load');
      loadModal.showModal();

      FF.on('progress', function(p) {
        updateFfmpegLoad('Downloading ffmpeg.wasm ... ' + Math.round(p.progress * 100) + '%', Math.round(p.progress * 100));
      });
      FF.on('log', function(l) { if (l.type === 'ffmpeg') Logger.log('[ffmpeg] ' + l.message); });

      var t0 = Date.now();
      await FF.load({ coreURL: './ffmpeg-core.js' });
      ffmpegReady = true;
      setPill($ffmpegStatus, 'ffmpeg ready', 'ready');
      favicon.set(':)');
      Logger.ok('ffmpeg.wasm loaded in ' + ((Date.now() - t0) / 1000).toFixed(1) + 's');
      loadModal.close();
    } catch(err) {
      setPill($ffmpegStatus, 'ffmpeg failed', 'error');
      favicon.blink(':(', 5000);
      Logger.err('ffmpeg load failed: ' + err.message);
      var m = document.getElementById('modal-ffmpeg-load');
      m.querySelector('.dialog-msg').textContent = 'ffmpeg.wasm failed to load. Transcoding unavailable.';
      m.querySelector('progress').remove();
    }
  };

  function runSelfCheck() {
    Logger.info('self-check: starting');
    var allOk = true;
    setPill($selfcheckStatus, 'testing', 'loading');

    var testH264 = new Uint8Array([
      0x00,0x00,0x00,0x01,0x67,0x42,0x00,0x0A,0xF8,0x0F,0x00,
      0x00,0x00,0x00,0x01,0x68,0xCE,0x06,0xE2,
      0x00,0x00,0x00,0x01,0x65,0x88,0x80,0x00,0x01,0x02,
      0x00,0x00,0x00,0x01,0x41,0x9A,0x00,0x00,0x04,
      0x00,0x00,0x00,0x01,0x41,0x9B,0x00,0x00,0x05
    ]);
    var ptr = WASM.alloc(testH264);
    var frames = WASM.h264Frames(ptr, testH264.length);
    var syncs = WASM.syncFrames(ptr, testH264.length);
    M._free(ptr);
    if (frames === 3 && syncs === 1) {
      Logger.ok('self-check: h264 parse OK (' + frames + '/' + syncs + ')');
    } else { Logger.err('self-check: h264 parse FAIL (' + frames + '/' + syncs + ')'); allOk = false; }

    var testBytes = new Uint8Array([0x12,0x34,0x56,0x78]);
    ptr = WASM.alloc(testBytes); M._free(ptr);
    Logger.ok('self-check: malloc/free OK');

    var testMps = new Uint8Array([0x00,0x00,0x01,0xBA,0x44,0x00,0x00,0x00,0x00,0x00]);
    ptr = WASM.alloc(testMps);
    var isMps = WASM.isMps(ptr, testMps.length); M._free(ptr);
    if (isMps) { Logger.ok('self-check: MPS detect OK'); }
    else { Logger.err('self-check: MPS detect FAIL'); allOk = false; }

    var testBad = new Uint8Array([0xFF,0xFF,0xFF,0xFF,0xFF]);
    ptr = WASM.alloc(testBad);
    var notMps = WASM.isMps(ptr, testBad.length); M._free(ptr);
    if (!notMps) { Logger.ok('self-check: MPS reject OK'); }
    else { Logger.err('self-check: MPS reject FAIL'); allOk = false; }

    if (allOk) {
      setPill($selfcheckStatus, 'all ok', 'ready');
      Logger.ok('self-check: ALL PASS');
    } else {
      setPill($selfcheckStatus, 'FAIL', 'error');
      favicon.blink(':(', 5000);
    }
  }

  PmfTools().then(function(mod) {
    M = mod;
    setPill($wasmStatus, 'pmftools ready', 'ready');
    favicon.set(':)');
    Logger.ok('pmftools WASM loaded');

    runSelfCheck();

    setupTranscode();
    setupDemux();
    setupMps2Pmf();
    setupAt3();
    setupInfo();
    setupH264();
    setupDragDrop();

    initFfmpeg();
  }).catch(function(err) {
    setPill($wasmStatus, 'pmftools failed', 'error');
    favicon.blink(':(', 5000);
    Logger.err('pmftools init failed: ' + err.message);
  });
})();
