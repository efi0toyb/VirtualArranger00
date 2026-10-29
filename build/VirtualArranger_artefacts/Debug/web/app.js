/* =============================================================================
   YAMAHA VIRTUAL ARRANGER WORKSTATION - FRONTEND LOGIC
   ============================================================================= */

// State Object
const state = {
  isPlaying: false,
  bpm: 120,
  chordName: '--',
  chordRoot: -1,
  chordType: '',
  bassNote: -1,
  sectionName: 'Main A',
  variationIndex: 1,
  autoFill: true,
  syncStart: false,
  syncStop: false,
  otsLink: true,
  singleFinger: false,
  splitPoint: 59, // B2
  masterVolume: 1.0,
  styleVolume: 0.85,
  leadVolume: 1.0,
  activeOTS: 0,
  otsData: [],
  sections: [],
  channelVolumes: new Array(16).fill(100),
  channelMutes: new Array(16).fill(false),
  channelSolos: new Array(16).fill(false),
  activeNotesLeft: [],
  activeNotesRight: [],
  styleLoaded: false,
  styleName: 'No Style Loaded',
  soundFontLoaded: false,
  soundFontName: 'None',
  activeTab: 'tab-home',
  lastBeatTick: 0
};

const NOTE_NAMES = ['C', 'C#', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B'];

// Channel Names for 8-Track Mixer (Channels 8 through 15)
const MIXER_TRACKS = [
  { ch: 8,  name: 'RHYTHM 1', short: 'RHY1', icon: '🥁' },
  { ch: 9,  name: 'RHYTHM 2', short: 'RHY2', icon: '🥁' },
  { ch: 10, name: 'BASS',     short: 'BASS', icon: '🎸' },
  { ch: 11, name: 'CHORD 1',  short: 'CHD1', icon: '🎹' },
  { ch: 12, name: 'CHORD 2',  short: 'CHD2', icon: '🎸' },
  { ch: 13, name: 'PAD',      short: 'PAD',  icon: '🎻' },
  { ch: 14, name: 'PHRASE 1', short: 'PHR1', icon: '🎺' },
  { ch: 15, name: 'PHRASE 2', short: 'PHR2', icon: '🎷' }
];

// GM Voices categorized by family
const GM_VOICES = {
  piano: [
    { prog: 0, name: 'Acoustic Grand Piano' },
    { prog: 1, name: 'Bright Acoustic Piano' },
    { prog: 2, name: 'Electric Grand Piano' },
    { prog: 3, name: 'Honky-tonk Piano' },
    { prog: 4, name: 'Electric Piano 1 (Rhodes)' },
    { prog: 5, name: 'Electric Piano 2 (DX)' },
    { prog: 6, name: 'Harpsichord' },
    { prog: 7, name: 'Clavinet' }
  ],
  organ: [
    { prog: 16, name: 'Drawbar Organ (Jazz)' },
    { prog: 17, name: 'Percussive Organ' },
    { prog: 18, name: 'Rock Organ' },
    { prog: 19, name: 'Church Organ' },
    { prog: 20, name: 'Reed Organ' },
    { prog: 21, name: 'Accordion' },
    { prog: 22, name: 'Harmonica' },
    { prog: 23, name: 'Tango Accordion' }
  ],
  guitar: [
    { prog: 24, name: 'Acoustic Guitar (Nylon)' },
    { prog: 25, name: 'Acoustic Guitar (Steel)' },
    { prog: 26, name: 'Electric Guitar (Jazz)' },
    { prog: 27, name: 'Electric Guitar (Clean)' },
    { prog: 28, name: 'Electric Guitar (Muted)' },
    { prog: 29, name: 'Overdriven Guitar' },
    { prog: 30, name: 'Distortion Guitar' },
    { prog: 31, name: 'Guitar Harmonics' }
  ],
  bass: [
    { prog: 32, name: 'Acoustic Bass' },
    { prog: 33, name: 'Electric Bass (Finger)' },
    { prog: 34, name: 'Electric Bass (Pick)' },
    { prog: 35, name: 'Fretless Bass' },
    { prog: 36, name: 'Slap Bass 1' },
    { prog: 37, name: 'Slap Bass 2' },
    { prog: 38, name: 'Synth Bass 1' },
    { prog: 39, name: 'Synth Bass 2' }
  ],
  strings: [
    { prog: 40, name: 'Violin' },
    { prog: 41, name: 'Viola' },
    { prog: 42, name: 'Cello' },
    { prog: 43, name: 'Contrabass' },
    { prog: 44, name: 'Tremolo Strings' },
    { prog: 45, name: 'Pizzicato Strings' },
    { prog: 46, name: 'Orchestral Harp' },
    { prog: 48, name: 'String Ensemble 1' },
    { prog: 49, name: 'String Ensemble 2' }
  ],
  brass: [
    { prog: 56, name: 'Trumpet' },
    { prog: 57, name: 'Trombone' },
    { prog: 58, name: 'Tuba' },
    { prog: 59, name: 'Muted Trumpet' },
    { prog: 60, name: 'French Horn' },
    { prog: 61, name: 'Brass Section' },
    { prog: 62, name: 'Synth Brass 1' },
    { prog: 63, name: 'Synth Brass 2' }
  ],
  reed: [
    { prog: 64, name: 'Soprano Sax' },
    { prog: 65, name: 'Alto Sax' },
    { prog: 66, name: 'Tenor Sax' },
    { prog: 67, name: 'Baritone Sax' },
    { prog: 68, name: 'Oboe' },
    { prog: 69, name: 'English Horn' },
    { prog: 70, name: 'Bassoon' },
    { prog: 71, name: 'Clarinet' },
    { prog: 73, name: 'Flute' },
    { prog: 75, name: 'Pan Flute' }
  ],
  lead: [
    { prog: 80, name: 'Lead 1 (Square)' },
    { prog: 81, name: 'Lead 2 (Sawtooth)' },
    { prog: 82, name: 'Lead 3 (Calliope)' },
    { prog: 84, name: 'Lead 5 (Charang)' },
    { prog: 85, name: 'Lead 6 (Voice)' },
    { prog: 87, name: 'Lead 8 (Bass + Lead)' }
  ],
  pad: [
    { prog: 88, name: 'Pad 1 (New Age)' },
    { prog: 89, name: 'Pad 2 (Warm)' },
    { prog: 90, name: 'Pad 3 (Polysynth)' },
    { prog: 91, name: 'Pad 4 (Choir)' },
    { prog: 92, name: 'Pad 5 (Bowed)' },
    { prog: 94, name: 'Pad 7 (Halo)' },
    { prog: 95, name: 'Pad 8 (Sweep)' }
  ],
  world: [
    { prog: 104, name: 'Sitar' },
    { prog: 105, name: 'Banjo' },
    { prog: 106, name: 'Shamisen' },
    { prog: 107, name: 'Koto' },
    { prog: 108, name: 'Kalimba' },
    { prog: 109, name: 'Bagpipe' },
    { prog: 110, name: 'Fiddle' },
    { prog: 111, name: 'Shanai' }
  ]
};

// =============================================================================
// INITIALIZATION
// =============================================================================
document.addEventListener('DOMContentLoaded', () => {
  buildPianoKeyboard();
  buildMixerStrips();
  populateVoiceList('piano');
  setupEventListeners();
  startStatusPolling();
  loadAudioSettings();
});

// =============================================================================
// PIANO KEYBOARD GENERATOR (61 Keys: C2 to C7 = MIDI 36 to 96)
// =============================================================================
function buildPianoKeyboard() {
  const container = document.getElementById('pianoKeyboard');
  container.innerHTML = '';

  const START_NOTE = 36; // C2
  const NUM_KEYS = 61;   // to C7 (96)

  const isBlack = (note) => {
    const pc = note % 12;
    return (pc === 1 || pc === 3 || pc === 6 || pc === 8 || pc === 10);
  };

  let totalWhite = 0;
  for (let n = START_NOTE; n < START_NOTE + NUM_KEYS; ++n) {
    if (!isBlack(n)) totalWhite++;
  }

  // 1. Create White Keys
  let whiteIndex = 0;
  for (let note = START_NOTE; note < START_NOTE + NUM_KEYS; ++note) {
    if (isBlack(note)) continue;

    const key = document.createElement('div');
    key.className = 'white-key';
    key.dataset.note = note;
    key.id = `key-${note}`;

    if (note < state.splitPoint)
      key.classList.add('is-left-zone');

    attachKeyEvents(key, note);
    container.appendChild(key);
    whiteIndex++;
  }

  // 2. Create Black Keys positioned absolutely
  whiteIndex = 0;
  const whiteKeyWidthPercent = 100.0 / totalWhite;
  const blackKeyWidthPercent = whiteKeyWidthPercent * 0.62;

  for (let note = START_NOTE; note < START_NOTE + NUM_KEYS; ++note) {
    if (!isBlack(note)) {
      whiteIndex++;
      continue;
    }

    const key = document.createElement('div');
    key.className = 'black-key';
    key.dataset.note = note;
    key.id = `key-${note}`;

    const leftPercent = (whiteIndex * whiteKeyWidthPercent) - (blackKeyWidthPercent * 0.5);
    key.style.left = `${leftPercent}%`;
    key.style.width = `${blackKeyWidthPercent}%`;

    if (note < state.splitPoint)
      key.classList.add('is-left-zone');

    attachKeyEvents(key, note);
    container.appendChild(key);
  }

  // 3. Create Split Marker Line
  const splitLine = document.createElement('div');
  splitLine.className = 'split-marker-line';
  splitLine.id = 'splitMarkerLine';
  container.appendChild(splitLine);
  updateSplitMarkerPosition();
}

function updateSplitMarkerPosition() {
  const line = document.getElementById('splitMarkerLine');
  if (!line) return;

  const START_NOTE = 36;
  const NUM_KEYS = 61;

  let totalWhite = 0;
  let splitWhite = 0;

  const isBlack = (note) => {
    const pc = note % 12;
    return (pc === 1 || pc === 3 || pc === 6 || pc === 8 || pc === 10);
  };

  for (let n = START_NOTE; n < START_NOTE + NUM_KEYS; ++n) {
    if (!isBlack(n)) {
      totalWhite++;
      if (n <= state.splitPoint) splitWhite++;
    }
  }

  const percent = (splitWhite / totalWhite) * 100.0;
  line.style.left = `${percent}%`;

  // Update visual key zones
  document.querySelectorAll('.white-key, .black-key').forEach(k => {
    const note = parseInt(k.dataset.note);
    if (note < state.splitPoint) {
      k.classList.add('is-left-zone');
    } else {
      k.classList.remove('is-left-zone');
    }
  });

  const noteName = `${NOTE_NAMES[state.splitPoint % 12]}${Math.floor(state.splitPoint / 12) - 1}`;
  document.getElementById('splitPointNoteName').textContent = `${noteName} (${state.splitPoint})`;
}

function attachKeyEvents(keyElement, noteNumber) {
  const triggerNote = (on) => {
    sendApi('/api/note', { note: noteNumber, velocity: 100, on });
  };

  keyElement.addEventListener('mousedown', (e) => {
    e.preventDefault();
    triggerNote(true);
  });

  keyElement.addEventListener('mouseup', (e) => {
    e.preventDefault();
    triggerNote(false);
  });

  keyElement.addEventListener('mouseleave', () => {
    triggerNote(false);
  });

  keyElement.addEventListener('touchstart', (e) => {
    e.preventDefault();
    triggerNote(true);
  });

  keyElement.addEventListener('touchend', (e) => {
    e.preventDefault();
    triggerNote(false);
  });
}

// =============================================================================
// 8-TRACK MIXER BUILDER
// =============================================================================
function buildMixerStrips() {
  const container = document.getElementById('mixerStrips');
  container.innerHTML = '';

  MIXER_TRACKS.forEach(track => {
    const strip = document.createElement('div');
    strip.className = 'mixer-strip';
    strip.dataset.channel = track.ch;

    strip.innerHTML = `
      <div class="strip-name">${track.icon} ${track.short}</div>
      <div class="strip-fader-area">
        <input type="range" class="strip-fader" min="0" max="127" value="100" id="fader-${track.ch}">
      </div>
      <span class="strip-val" id="fader-val-${track.ch}">100</span>
      <div class="strip-buttons">
        <button class="strip-btn mute" id="mute-${track.ch}">M</button>
        <button class="strip-btn solo" id="solo-${track.ch}">S</button>
      </div>
    `;

    container.appendChild(strip);

    const fader = strip.querySelector(`#fader-${track.ch}`);
    fader.addEventListener('input', (e) => {
      const val = parseInt(e.target.value);
      strip.querySelector(`#fader-val-${track.ch}`).textContent = val;
      sendApi('/api/mixer', { channel: track.ch, volume: val });
    });

    const muteBtn = strip.querySelector(`#mute-${track.ch}`);
    muteBtn.addEventListener('click', () => {
      const isMuted = !muteBtn.classList.contains('active');
      muteBtn.classList.toggle('active', isMuted);
      sendApi('/api/mixer', { channel: track.ch, mute: isMuted });
    });

    const soloBtn = strip.querySelector(`#solo-${track.ch}`);
    soloBtn.addEventListener('click', () => {
      const isSoloed = !soloBtn.classList.contains('active');
      soloBtn.classList.toggle('active', isSoloed);
      sendApi('/api/mixer', { channel: track.ch, solo: isSoloed });
    });
  });
}

// =============================================================================
// VOICES BROWSER
// =============================================================================
function populateVoiceList(family) {
  const container = document.getElementById('voiceList');
  container.innerHTML = '';

  const list = GM_VOICES[family] || [];
  list.forEach(v => {
    const item = document.createElement('div');
    item.className = 'voice-item';
    item.dataset.prog = v.prog;
    item.innerHTML = `
      <div class="item-name">${v.name}</div>
      <div class="item-meta">Program: ${v.prog} | Bank: GM</div>
    `;

    item.addEventListener('click', () => {
      document.querySelectorAll('.voice-item').forEach(el => el.classList.remove('selected'));
      item.classList.add('selected');
      sendApi('/api/lead-voice', { program: v.prog });
    });

    container.appendChild(item);
  });
}

// =============================================================================
// EVENT LISTENERS SETUP
// =============================================================================
function setupEventListeners() {
  // Navigation Tabs
  document.querySelectorAll('.screen-tab').forEach(tab => {
    tab.addEventListener('click', () => {
      document.querySelectorAll('.screen-tab').forEach(t => t.classList.remove('active'));
      document.querySelectorAll('.screen-tab-pane').forEach(p => p.classList.remove('active'));

      tab.classList.add('active');
      const targetPane = document.getElementById(tab.dataset.tab);
      if (targetPane) targetPane.classList.add('active');
      state.activeTab = tab.dataset.tab;

      if (state.activeTab === 'tab-styles') loadStylesDirectory();
    });
  });

  // Voice Category Sidebar
  document.querySelectorAll('#voiceCategoryList li').forEach(li => {
    li.addEventListener('click', () => {
      document.querySelectorAll('#voiceCategoryList li').forEach(el => el.classList.remove('active'));
      li.classList.add('active');
      populateVoiceList(li.dataset.family);
    });
  });

  // Transport: Start / Stop
  const btnStartStop = document.getElementById('btnStartStop');
  btnStartStop.addEventListener('click', () => {
    const action = state.isPlaying ? 'stop' : 'play';
    sendApi('/api/transport', { action });
  });

  // Tempo Buttons
  document.getElementById('btnTempoDown').addEventListener('click', () => {
    sendApi('/api/transport', { action: 'set_bpm', value: Math.max(30, state.bpm - 1) });
  });
  document.getElementById('btnTempoUp').addEventListener('click', () => {
    sendApi('/api/transport', { action: 'set_bpm', value: Math.min(300, state.bpm + 1) });
  });

  // Tap Tempo
  let tapTimes = [];
  document.getElementById('btnTempoTap').addEventListener('click', () => {
    const now = Date.now();
    tapTimes.push(now);
    if (tapTimes.length > 4) tapTimes.shift();

    if (tapTimes.length >= 2) {
      let intervals = [];
      for (let i = 1; i < tapTimes.length; ++i) intervals.push(tapTimes[i] - tapTimes[i - 1]);
      const avgMs = intervals.reduce((a, b) => a + b, 0) / intervals.length;
      if (avgMs > 150 && avgMs < 2000) {
        const calculatedBpm = Math.round(60000 / avgMs);
        sendApi('/api/transport', { action: 'set_bpm', value: calculatedBpm });
      }
    }
  });

  // Arranger Physical Buttons
  document.querySelectorAll('.arranger-btn[data-type]').forEach(btn => {
    btn.addEventListener('click', () => {
      const type = btn.dataset.type;
      const index = parseInt(btn.dataset.index || 1);
      sendApi('/api/section', { type, index });
    });
  });

  // OTS Quick Buttons
  document.querySelectorAll('.ots-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      const idx = parseInt(btn.dataset.ots);
      sendApi('/api/ots', { index: idx });
    });
  });

  // Left Flank Volume Sliders
  document.getElementById('masterVolSlider').addEventListener('input', (e) => {
    const val = parseFloat(e.target.value);
    document.getElementById('masterVolVal').textContent = `${Math.round(val * 100)}%`;
    sendApi('/api/mixer', { masterVolume: val });
  });
  document.getElementById('styleVolSlider').addEventListener('input', (e) => {
    const val = parseFloat(e.target.value);
    document.getElementById('styleVolVal').textContent = `${Math.round(val * 100)}%`;
    sendApi('/api/mixer', { styleVolume: val });
  });
  document.getElementById('leadVolSlider').addEventListener('input', (e) => {
    const val = parseFloat(e.target.value);
    document.getElementById('leadVolVal').textContent = `${Math.round(val * 100)}%`;
    sendApi('/api/mixer', { leadVolume: val });
  });

  // Left Flank Switches
  setupToggle('btnSyncStart', 'set_sync_start', 'syncStart');
  setupToggle('btnSyncStop', 'set_sync_stop', 'syncStop');
  setupToggle('btnAutoFill', 'set_autofill', 'autoFill');
  setupToggle('btnOtsLink', 'set_ots_link', 'otsLink');
  setupToggle('btnSingleFinger', 'set_single_finger', 'singleFinger');

  // Split Point Slider
  const splitSlider = document.getElementById('splitPointSlider');
  splitSlider.addEventListener('input', (e) => {
    const note = parseInt(e.target.value);
    state.splitPoint = note;
    updateSplitMarkerPosition();
    sendApi('/api/transport', { action: 'set_split_point', value: note });
  });

  // Modals & Header Actions
  const settingsModal = document.getElementById('settingsModal');
  document.getElementById('btnOpenSettings').addEventListener('click', () => {
    loadAudioSettings();
    settingsModal.classList.add('open');
  });
  document.getElementById('btnCloseSettings').addEventListener('click', () => {
    settingsModal.classList.remove('open');
  });
  document.getElementById('btnCancelSettings').addEventListener('click', () => {
    settingsModal.classList.remove('open');
  });
  document.getElementById('btnApplySettings').addEventListener('click', () => {
    applyAudioSettings();
  });

  document.getElementById('btnBrowseStyles').addEventListener('click', () => {
    document.querySelector('.screen-tab[data-tab="tab-styles"]').click();
  });

  document.getElementById('btnLoadSoundFont').addEventListener('click', () => {
    loadAudioSettings();
    document.getElementById('settingsModal').classList.add('open');
    setTimeout(() => {
      const sfInput = document.getElementById('sf2PathInput');
      if (sfInput) sfInput.focus();
    }, 200);
  });

  document.getElementById('btnSelectSf2File').addEventListener('click', async () => {
    try {
      const res = await fetch('/api/choose-soundfont');
      if (res.ok) {
        const data = await res.json();
        if (data.success) {
          document.getElementById('sf2PathInput').value = data.path;
          alert('SoundFont loaded successfully: ' + data.name);
        }
      }
    } catch (e) {
      console.warn('Choose SoundFont error:', e);
    }
  });

  const btnPickFolder = document.getElementById('btnPickStylesFolder');
  if (btnPickFolder) {
    btnPickFolder.addEventListener('click', async () => {
      try {
        const res = await fetch('/api/choose-folder');
        if (res.ok) {
          const data = await res.json();
          if (!data.cancelled) {
            renderStylesData(data);
          }
        }
      } catch (e) {
        console.warn('Choose folder error:', e);
      }
    });
  }

  const btnSingleStyle = document.getElementById('btnBrowseSingleStyle');
  if (btnSingleStyle) {
    btnSingleStyle.addEventListener('click', async () => {
      try {
        const res = await fetch('/api/choose-style');
        if (res.ok) {
          const data = await res.json();
          if (data.success) {
            document.querySelector('.screen-tab[data-tab="tab-home"]').click();
          }
        }
      } catch (e) {
        console.warn('Choose style error:', e);
      }
    });
  }

  const btnUp = document.getElementById('btnDirUp');
  if (btnUp) {
    btnUp.addEventListener('click', () => {
      if (state.parentDir) {
        loadStylesDirectory(state.parentDir);
      }
    });
  }

  const searchInput = document.getElementById('styleSearchInput');
  if (searchInput) {
    searchInput.addEventListener('input', (e) => {
      const q = e.target.value.toLowerCase().trim();
      document.querySelectorAll('#styleFileList .file-item').forEach(item => {
        const name = item.querySelector('.item-name')?.textContent.toLowerCase() || '';
        item.style.display = (!q || name.includes(q)) ? '' : 'none';
      });
    });
  }

  // Driver Change in Modal: reload devices
  document.getElementById('audioDriverSelect').addEventListener('change', (e) => {
    refreshDevicesForDriver(e.target.value);
  });
}

function setupToggle(elementId, apiAction, stateKey) {
  const btn = document.getElementById(elementId);
  btn.addEventListener('click', () => {
    const newVal = !state[stateKey];
    state[stateKey] = newVal;
    btn.classList.toggle('active', newVal);
    sendApi('/api/transport', { action: apiAction, value: newVal });
  });
}

// =============================================================================
// API HELPERS & STATUS POLLING
// =============================================================================
async function sendApi(endpoint, data = {}) {
  try {
    const res = await fetch(endpoint, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(data)
    });
    return await res.json();
  } catch (err) {
    console.warn(`API Error on ${endpoint}:`, err);
  }
}

function startStatusPolling() {
  setInterval(async () => {
    try {
      const res = await fetch('/api/status');
      if (res.ok) {
        const data = await res.json();
        updateUIFromStatus(data);
      }
    } catch (e) {
      // Server reconnecting
    }
  }, 50);
}

// =============================================================================
// UPDATE UI FROM SERVER STATUS
// =============================================================================
function updateUIFromStatus(data) {
  // 1. Play / Stop Button State
  state.isPlaying = data.isPlaying;
  const btnStartStop = document.getElementById('btnStartStop');
  const textStartStop = document.getElementById('startStopText');
  if (state.isPlaying) {
    btnStartStop.classList.add('playing');
    textStartStop.textContent = 'STOP';
  } else {
    btnStartStop.classList.remove('playing');
    textStartStop.textContent = 'START';
  }

  // 2. Tempo & Beat Indicator
  state.bpm = Math.round(data.bpm);
  document.getElementById('displayBpm').textContent = state.bpm;

  // Pulsing Beat animation
  if (state.isPlaying) {
    const beatPeriodMs = (60000 / state.bpm);
    const beatIndex = Math.floor((Date.now() / beatPeriodMs) % 4);
    for (let b = 1; b <= 4; ++b) {
      const el = document.getElementById(`beat${b}`);
      if (b === (beatIndex + 1)) {
        el.className = (b === 1) ? 'beat-light active-1' : 'beat-light active-other';
      } else {
        el.className = 'beat-light';
      }
    }
  } else {
    for (let b = 1; b <= 4; ++b) document.getElementById(`beat${b}`).className = 'beat-light';
  }

  // 3. Chord Display
  state.chordName = data.chordName;
  const chordEl = document.getElementById('displayChordName');
  chordEl.textContent = state.chordName;

  const chordDetailEl = document.getElementById('displayChordDetail');
  if (data.chordRoot >= 0) {
    chordDetailEl.textContent = `Root: ${NOTE_NAMES[data.chordRoot]} | Type: ${data.chordType} ${data.bassNote >= 0 && data.bassNote !== data.chordRoot ? '| Bass: ' + NOTE_NAMES[data.bassNote] : ''}`;
  } else {
    chordDetailEl.textContent = 'Play chord in Left Hand (C2 - B2)';
  }

  // 4. Style & Section Display
  document.getElementById('displayStyleTitle').textContent = data.styleName || 'Standard Style';
  document.getElementById('displaySectionTag').textContent = data.sectionName || 'MAIN A';
  document.getElementById('headerStyleName').textContent = data.styleName || 'None';
  document.getElementById('headerSoundFontName').textContent = data.soundFontName || 'None';

  // 5. Active Arranger Button Highlighting
  highlightActiveArrangerSection(data.sectionName);

  // 6. OTS Buttons
  state.activeOTS = data.activeOTS;
  document.querySelectorAll('.ots-btn').forEach((b, idx) => {
    b.classList.toggle('active', idx === state.activeOTS);
  });
  if (data.ots && data.ots.length === 4) {
    for (let i = 0; i < 4; ++i) {
      const nameEl = document.getElementById(`otsName${i + 1}`);
      if (nameEl) nameEl.textContent = data.ots[i].r1Name;
    }
  }

  // 7. Toggle Button States
  document.getElementById('btnAutoFill').classList.toggle('active', data.autoFill);
  document.getElementById('btnSyncStart').classList.toggle('active', data.syncStart);
  document.getElementById('btnSyncStop').classList.toggle('active', data.syncStop);
  document.getElementById('btnOtsLink').classList.toggle('active', data.otsLink);
  document.getElementById('btnSingleFinger').classList.toggle('active', data.singleFinger);

  // 8. Split Point Slider
  if (data.splitPoint !== state.splitPoint) {
    state.splitPoint = data.splitPoint;
    document.getElementById('splitPointSlider').value = data.splitPoint;
    updateSplitMarkerPosition();
  }

  // 9. Real-Time Key Illumination
  updateKeyboardActiveNotes(data.activeNotesLeft || [], data.activeNotesRight || []);

  // 10. Mixer Faders
  if (data.channelVolumes) {
    MIXER_TRACKS.forEach(track => {
      const fader = document.getElementById(`fader-${track.ch}`);
      const valLabel = document.getElementById(`fader-val-${track.ch}`);
      if (fader && document.activeElement !== fader) {
        fader.value = data.channelVolumes[track.ch];
        valLabel.textContent = data.channelVolumes[track.ch];
      }

      const muteBtn = document.getElementById(`mute-${track.ch}`);
      if (muteBtn) muteBtn.classList.toggle('active', !!data.channelMutes[track.ch]);

      const soloBtn = document.getElementById(`solo-${track.ch}`);
      if (soloBtn) soloBtn.classList.toggle('active', !!data.channelSolos[track.ch]);
    });
  }
}

function highlightActiveArrangerSection(sectionName) {
  const name = (sectionName || '').toLowerCase().replace(/\s+/g, '');

  document.querySelectorAll('.arranger-btn[data-type]').forEach(btn => {
    btn.classList.remove('active');
  });

  if (name.includes('main')) {
    if (name.includes('b') || name.includes('2')) document.getElementById('btnMain2').classList.add('active');
    else if (name.includes('c') || name.includes('3')) document.getElementById('btnMain3').classList.add('active');
    else if (name.includes('d') || name.includes('4')) document.getElementById('btnMain4').classList.add('active');
    else document.getElementById('btnMain1').classList.add('active');
  } else if (name.includes('intro')) {
    if (name.includes('2')) document.getElementById('btnIntro2').classList.add('active');
    else if (name.includes('3')) document.getElementById('btnIntro3').classList.add('active');
    else document.getElementById('btnIntro1').classList.add('active');
  } else if (name.includes('ending')) {
    if (name.includes('2')) document.getElementById('btnEnding2').classList.add('active');
    else if (name.includes('3')) document.getElementById('btnEnding3').classList.add('active');
    else document.getElementById('btnEnding1').classList.add('active');
  } else if (name.includes('break')) {
    document.getElementById('btnBreak').classList.add('active');
  } else if (name.includes('fill')) {
    document.getElementById('btnFill').classList.add('active');
  }
}

function updateKeyboardActiveNotes(leftNotes, rightNotes) {
  document.querySelectorAll('.white-key, .black-key').forEach(k => {
    k.classList.remove('pressed-left', 'pressed-right');
  });

  leftNotes.forEach(note => {
    const k = document.getElementById(`key-${note}`);
    if (k) k.classList.add('pressed-left');
  });

  rightNotes.forEach(note => {
    const k = document.getElementById(`key-${note}`);
    if (k) k.classList.add('pressed-right');
  });
}

// =============================================================================
// STYLES BROWSER LOADER & SUBFOLDER NAVIGATION
// =============================================================================
async function loadStylesDirectory(dirPath = '') {
  try {
    const url = `/api/browse-styles${dirPath ? '?dir=' + encodeURIComponent(dirPath) : ''}`;
    const res = await fetch(url);
    if (res.ok) {
      const data = await res.json();
      renderStylesData(data);
    }
  } catch (e) {
    console.warn('Failed to load styles:', e);
  }
}

function renderStylesData(data) {
  state.currentStyleDir = data.currentDir || '';
  state.parentDir = data.parentDir || '';

  // 1. Update Dir Label & Up Button
  const lblDir = document.getElementById('lblCurrentDir');
  if (lblDir) lblDir.textContent = data.currentDir || 'None';

  const btnUp = document.getElementById('btnDirUp');
  if (btnUp) btnUp.disabled = !data.parentDir;

  // 2. Render Subfolders
  const subContainer = document.getElementById('subfoldersContainer');
  if (subContainer) {
    subContainer.innerHTML = '';
    if (data.folders && data.folders.length > 0) {
      data.folders.forEach(f => {
        const chip = document.createElement('div');
        chip.className = 'folder-chip';
        chip.innerHTML = `<span>📁 ${f.name}</span>`;
        chip.title = f.path;
        chip.addEventListener('click', () => {
          loadStylesDirectory(f.path);
        });
        subContainer.appendChild(chip);
      });
    }
  }

  // 3. Render Style Files
  const fileContainer = document.getElementById('styleFileList');
  if (fileContainer) {
    fileContainer.innerHTML = '';

    if (!data.files || data.files.length === 0) {
      fileContainer.innerHTML = '<div class="empty-state">No style files found in this folder or its subfolders. Click "📂 Choose Folder..." to select your styles collection.</div>';
      return;
    }

    data.files.forEach(f => {
      const item = document.createElement('div');
      item.className = 'file-item';
      item.dataset.path = f.path;

      const subTag = f.subfolder ? `<span class="item-subfolder-tag">📁 ${f.subfolder}</span>` : '';
      const sizeKb = Math.round((f.size || 0) / 1024);

      item.innerHTML = `
        <div class="item-name">${subTag}${f.name}</div>
        <div class="item-meta">${sizeKb} KB | Yamaha Style</div>
      `;

      item.addEventListener('click', async () => {
        document.querySelectorAll('.file-item').forEach(el => el.classList.remove('selected'));
        item.classList.add('selected');
        const res = await sendApi('/api/load-style', { path: f.path });
        if (res && res.success) {
          document.querySelector('.screen-tab[data-tab="tab-home"]').click();
        } else {
          alert('Failed to load style file: ' + (res ? res.error : 'Unknown error'));
        }
      });

      fileContainer.appendChild(item);
    });
  }
}

// =============================================================================
// AUDIO DRIVER & DEVICE SETTINGS
// =============================================================================
let audioSettingsData = null;

async function loadAudioSettings() {
  try {
    const res = await fetch('/api/audio-settings');
    if (!res.ok) return;

    audioSettingsData = await res.json();

    // Populate Drivers
    const driverSelect = document.getElementById('audioDriverSelect');
    driverSelect.innerHTML = '';
    (audioSettingsData.drivers || []).forEach(d => {
      const opt = document.createElement('option');
      opt.value = d;
      opt.textContent = d;
      if (d === audioSettingsData.currentDriver) opt.selected = true;
      driverSelect.appendChild(opt);
    });

    // Populate Devices
    populateDeviceSelect(audioSettingsData.devices || [], audioSettingsData.currentDevice);

    // Populate Sample Rates
    const rateSelect = document.getElementById('sampleRateSelect');
    rateSelect.innerHTML = '';
    (audioSettingsData.sampleRates || [44100, 48000]).forEach(r => {
      const opt = document.createElement('option');
      opt.value = r;
      opt.textContent = `${Number(r).toLocaleString()} Hz`;
      if (Math.abs(r - audioSettingsData.currentSampleRate) < 1.0) opt.selected = true;
      rateSelect.appendChild(opt);
    });

    // Populate Buffer Sizes
    const bufSelect = document.getElementById('bufferSizeSelect');
    bufSelect.innerHTML = '';
    (audioSettingsData.bufferSizes || [64, 128, 256, 512, 1024]).forEach(b => {
      const opt = document.createElement('option');
      opt.value = b;
      const ms = ((b / (audioSettingsData.currentSampleRate || 44100)) * 1000).toFixed(1);
      opt.textContent = `${b} samples (~${ms} ms)`;
      if (b === audioSettingsData.currentBufferSize) opt.selected = true;
      bufSelect.appendChild(opt);
    });

    // Populate Detected SoundFonts
    try {
      const sfRes = await fetch('/api/browse-soundfonts');
      if (sfRes.ok) {
        const sfData = await sfRes.json();
        const sfContainer = document.getElementById('detectedSoundFontsList');
        if (sfContainer) {
          sfContainer.innerHTML = '';
          if (sfData.soundfonts && sfData.soundfonts.length > 0) {
            const label = document.createElement('div');
            label.style.fontSize = '12px';
            label.style.color = '#888';
            label.style.marginBottom = '4px';
            label.textContent = 'Detected SoundFonts on your system:';
            sfContainer.appendChild(label);

            sfData.soundfonts.forEach(sf => {
              const row = document.createElement('div');
              row.className = 'sf-quick-item';
              row.innerHTML = `
                <span>🎵 <strong>${sf.name}</strong> (${Math.round(sf.size / (1024 * 1024))} MB)</span>
                <button class="sf-quick-btn">Load</button>
              `;
              row.querySelector('.sf-quick-btn').addEventListener('click', async () => {
                document.getElementById('sf2PathInput').value = sf.path;
                const res = await sendApi('/api/load-soundfont', { path: sf.path });
                if (res && res.success) {
                  alert('SoundFont loaded: ' + sf.name);
                } else {
                  alert('Failed to load SoundFont');
                }
              });
              sfContainer.appendChild(row);
            });
          }
        }
      }
    } catch (e) {
      console.warn('Could not browse soundfonts:', e);
    }

    // Populate MIDI Inputs
    const midiList = document.getElementById('midiDeviceList');
    midiList.innerHTML = '';
    if (!audioSettingsData.midiInputs || audioSettingsData.midiInputs.length === 0) {
      midiList.innerHTML = '<div class="empty-state-sm">No MIDI input keyboards detected. Connect a USB MIDI controller.</div>';
    } else {
      audioSettingsData.midiInputs.forEach(m => {
        const row = document.createElement('label');
        row.className = 'midi-device-item';
        row.innerHTML = `
          <input type="checkbox" data-midi-id="${m.id}" ${m.enabled ? 'checked' : ''}>
          <span>🎹 ${m.name}</span>
        `;
        midiList.appendChild(row);
      });
    }

  } catch (err) {
    console.warn('Error loading audio settings:', err);
  }
}

function populateDeviceSelect(devices, currentDevice) {
  const devSelect = document.getElementById('audioDeviceSelect');
  devSelect.innerHTML = '';
  devices.forEach(dev => {
    const opt = document.createElement('option');
    opt.value = dev;
    opt.textContent = dev;
    if (dev === currentDevice) opt.selected = true;
    devSelect.appendChild(opt);
  });
}

function refreshDevicesForDriver(driverName) {
  // If we already have device data, select default or request updated
  loadAudioSettings();
}

async function applyAudioSettings() {
  const driver = document.getElementById('audioDriverSelect').value;
  const device = document.getElementById('audioDeviceSelect').value;
  const sampleRate = parseFloat(document.getElementById('sampleRateSelect').value);
  const bufferSize = parseInt(document.getElementById('bufferSizeSelect').value);

  const midiDevices = [];
  document.querySelectorAll('#midiDeviceList input[type="checkbox"]').forEach(cb => {
    midiDevices.push({
      id: cb.dataset.midiId,
      enabled: cb.checked
    });
  });

  const res = await sendApi('/api/audio-settings', {
    driver,
    device,
    sampleRate,
    bufferSize,
    midiDevices
  });

  if (res && res.success) {
    document.getElementById('settingsModal').classList.remove('open');
  } else {
    alert('Failed to apply audio settings. Device may be in exclusive use by another application.');
  }
}
