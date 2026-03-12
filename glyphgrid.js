const TAU = Math.PI * 2;
const COLS = 12;
const ROWS = 8;

const glyphTypes = {
  empty: { label: "empty", short: "·", color: "transparent", defaultCode: "0" },
  pulse: { label: "pulse", short: "P", color: "#ffd449", defaultCode: "1" },
  tone: { label: "tone", short: "T", color: "#00e5ff", defaultCode: "220 + row * 35" },
  noise: { label: "noise", short: "N", color: "#ff8a3d", defaultCode: "0.28" },
  mul: { label: "mul", short: "×", color: "#7cff8a", defaultCode: "0.7" },
  bias: { label: "bias", short: "+", color: "#ff5ca8", defaultCode: "0.12" },
  hue: { label: "hue", short: "H", color: "#bb86ff", defaultCode: "0.56 + row * 0.03" },
  audio: { label: "audio", short: "A", color: "#00ffd5", defaultCode: "1" },
  visual: { label: "visual", short: "V", color: "#ff67d9", defaultCode: "1" },
};

const state = {
  bpm: 108,
  currentTime: 0,
  running: false,
  selectedTool: "tone",
  selectedCellId: "1:0",
  audioLevel: 0,
  visualState: { hue: 0.58, energy: 0, lines: [] },
  grid: createDefaultGrid(),
};

const dom = {
  grid: document.querySelector("#grid"),
  palette: document.querySelector("#tool-palette"),
  inspectorTitle: document.querySelector("#inspector-title"),
  inspectorBody: document.querySelector("#inspector-body"),
  playToggle: document.querySelector("#play-toggle"),
  clearButton: document.querySelector("#clear-button"),
  bpmReadout: document.querySelector("#bpm-readout"),
  beatReadout: document.querySelector("#beat-readout"),
  toolReadout: document.querySelector("#tool-readout"),
  audioReadout: document.querySelector("#audio-readout"),
  columnReadout: document.querySelector("#column-readout"),
  fpsChip: document.querySelector("#fps-chip"),
  stage: document.querySelector("#stage"),
};

const ctx = dom.stage.getContext("2d");
let audioEngine = null;
let lastFrame = performance.now();
let fpsFrames = 0;
let fpsWindowStart = performance.now();

bootstrap();

function bootstrap() {
  renderPalette();
  renderGrid();
  renderInspector();
  bindEvents();
  requestAnimationFrame(frame);
}

function createDefaultGrid() {
  const grid = [];
  for (let row = 0; row < ROWS; row += 1) {
    for (let col = 0; col < COLS; col += 1) {
      grid.push(makeCell(row, col, "empty"));
    }
  }

  stamp(0, 0, "pulse", "1");
  stamp(0, 1, "tone", "180");
  stamp(0, 2, "mul", "0.8");
  stamp(0, 3, "audio", "1");

  stamp(1, 2, "pulse", "1");
  stamp(1, 3, "tone", "260");
  stamp(1, 4, "audio", "1");

  stamp(2, 4, "pulse", "1");
  stamp(2, 5, "tone", "320");
  stamp(2, 6, "audio", "1");

  stamp(4, 0, "pulse", "1");
  stamp(4, 1, "noise", "0.18");
  stamp(4, 2, "audio", "1");

  stamp(5, 6, "pulse", "1");
  stamp(5, 7, "hue", "0.72");
  stamp(5, 8, "visual", "1");

  stamp(6, 8, "pulse", "1");
  stamp(6, 9, "tone", "420");
  stamp(6, 10, "bias", "0.18");
  stamp(6, 11, "visual", "1");

  return grid;

  function stamp(row, col, type, code) {
    const cell = grid.find((entry) => entry.row === row && entry.col === col);
    cell.type = type;
    cell.code = code;
    cell.label = glyphTypes[type].label;
  }
}

function makeCell(row, col, type) {
  return {
    id: `${row}:${col}`,
    row,
    col,
    type,
    label: glyphTypes[type].label,
    code: glyphTypes[type].defaultCode,
  };
}

function renderPalette() {
  dom.palette.innerHTML = Object.entries(glyphTypes)
    .filter(([key]) => key !== "empty")
    .map(
      ([key, value]) => `
        <button class="tool-button ${state.selectedTool === key ? "active" : ""}" data-tool="${key}" type="button">
          ${value.short} ${value.label}
        </button>
      `,
    )
    .join("");

  dom.palette.querySelectorAll("[data-tool]").forEach((button) => {
    button.addEventListener("click", () => {
      state.selectedTool = button.dataset.tool;
      dom.toolReadout.textContent = state.selectedTool;
      renderPalette();
    });
  });
}

function renderGrid() {
  const currentStep = getCurrentStep();
  dom.columnReadout.textContent = `Step ${currentStep + 1}`;
  dom.grid.innerHTML = state.grid
    .map((cell) => {
      const type = glyphTypes[cell.type];
      return `
        <button
          class="cell ${cell.type} ${cell.type === "empty" ? "empty" : ""} ${cell.col === currentStep ? "active-col" : ""} ${state.selectedCellId === cell.id ? "selected" : ""}"
          data-cell-id="${cell.id}"
          type="button"
        >
          <div class="cell-label">${type.short} ${cell.row + 1}:${cell.col + 1}</div>
          <div class="cell-code">${escapeHtml(cell.code)}</div>
        </button>
      `;
    })
    .join("");

  dom.grid.querySelectorAll("[data-cell-id]").forEach((button) => {
    button.addEventListener("click", () => onCellClick(button.dataset.cellId));
  });
}

function onCellClick(cellId) {
  const cell = state.grid.find((entry) => entry.id === cellId);
  if (!cell) return;
  if (cell.type !== state.selectedTool) {
    cell.type = state.selectedTool;
    cell.label = glyphTypes[state.selectedTool].label;
    if (cell.code === glyphTypes.empty.defaultCode || cell.type !== state.selectedTool) {
      cell.code = glyphTypes[state.selectedTool].defaultCode;
    }
  }
  state.selectedCellId = cellId;
  renderGrid();
  renderInspector();
}

function renderInspector() {
  const cell = state.grid.find((entry) => entry.id === state.selectedCellId);
  if (!cell) {
    dom.inspectorTitle.textContent = "Select a cell";
    dom.inspectorBody.className = "inspector-body empty-state";
    dom.inspectorBody.textContent = "Click a grid cell to edit it.";
    return;
  }

  dom.inspectorTitle.textContent = `${glyphTypes[cell.type].label} ${cell.row + 1}:${cell.col + 1}`;
  dom.inspectorBody.className = "inspector-body";
  dom.inspectorBody.innerHTML = `
    <div class="field">
      <label>Glyph</label>
      <input value="${glyphTypes[cell.type].label}" disabled />
    </div>
    <div class="field">
      <label>Label</label>
      <input id="cell-label" value="${cell.label}" />
    </div>
    <div class="field">
      <label>Expression</label>
      <textarea id="cell-code">${cell.code}</textarea>
    </div>
    <div class="field">
      <label>Clear</label>
      <button id="erase-cell" class="transport-button secondary" type="button">Erase Cell</button>
    </div>
  `;

  dom.inspectorBody.querySelector("#cell-label").addEventListener("input", (event) => {
    cell.label = event.currentTarget.value;
  });
  dom.inspectorBody.querySelector("#cell-code").addEventListener("change", (event) => {
    cell.code = event.currentTarget.value;
    renderGrid();
  });
  dom.inspectorBody.querySelector("#erase-cell").addEventListener("click", () => {
    cell.type = "empty";
    cell.label = glyphTypes.empty.label;
    cell.code = glyphTypes.empty.defaultCode;
    renderGrid();
    renderInspector();
  });
}

function bindEvents() {
  dom.playToggle.addEventListener("click", togglePlayback);
  dom.clearButton.addEventListener("click", () => {
    state.grid = state.grid.map((cell) => makeCell(cell.row, cell.col, "empty"));
    renderGrid();
    renderInspector();
  });
}

function togglePlayback() {
  state.running = !state.running;
  dom.playToggle.textContent = state.running ? "Stop" : "Play";
  dom.audioReadout.textContent = state.running ? "running" : "stopped";
  if (state.running) {
    if (!audioEngine) audioEngine = createAudioEngine();
    audioEngine.start();
  } else if (audioEngine) {
    audioEngine.stop();
  }
}

function frame(now) {
  const dt = Math.max(1 / 240, (now - lastFrame) / 1000);
  if (state.running) state.currentTime += dt;
  const beat = state.currentTime * (state.bpm / 60);
  dom.beatReadout.textContent = beat.toFixed(2);
  dom.bpmReadout.textContent = state.bpm.toFixed(0);

  fpsFrames += 1;
  if (now - fpsWindowStart > 500) {
    dom.fpsChip.textContent = `FPS ${Math.round((fpsFrames * 1000) / (now - fpsWindowStart))}`;
    fpsWindowStart = now;
    fpsFrames = 0;
  }

  const score = evaluateScore(state.currentTime);
  state.audioLevel = score.audio;
  state.visualState = score.visual;
  renderStage(getCurrentStep(), score);
  renderGrid();

  lastFrame = now;
  requestAnimationFrame(frame);
}

function evaluateScore(t) {
  const activeStep = getCurrentStep(t);
  let audio = 0;
  let hue = 0.58;
  let visualEnergy = 0;
  const lines = [];

  for (let row = 0; row < ROWS; row += 1) {
    let x = 0;
    let rowHue = hue;
    let rowVisual = 0;
    const rowCells = state.grid
      .filter((cell) => cell.row === row)
      .sort((a, b) => a.col - b.col);

    for (const cell of rowCells) {
      const value = evaluateExpr(cell.code, { x, t, row, col: cell.col, beat: t * (state.bpm / 60) });
      switch (cell.type) {
        case "pulse":
          x = activeStep === cell.col ? value : 0;
          break;
        case "tone":
          x = Math.sin(t * TAU * Math.max(1, value)) * (activeStep === cell.col ? 1 : 0);
          break;
        case "noise":
          x = (((fract(Math.sin((t * 1000 + row * 17 + cell.col * 11)) * 43758.5453) * 2) - 1) * value) * (activeStep === cell.col ? 1 : 0);
          break;
        case "mul":
          x *= value;
          break;
        case "bias":
          x += value;
          break;
        case "hue":
          rowHue = value;
          break;
        case "audio":
          audio += x * value;
          break;
        case "visual":
          rowVisual += Math.abs(x) * value;
          visualEnergy += Math.abs(x) * value;
          lines.push({ row, hue: rowHue, energy: Math.abs(x) * value, step: cell.col });
          break;
        default:
          break;
      }
    }

    hue = rowHue;
    visualEnergy += rowVisual * 0.35;
  }

  return {
    audio: clamp(audio * 0.45, -1, 1),
    visual: {
      hue,
      energy: clamp(visualEnergy, 0, 1.4),
      lines,
    },
  };
}

function renderStage(activeStep, score) {
  const { width, height } = dom.stage;
  const grad = ctx.createLinearGradient(0, 0, width, height);
  grad.addColorStop(0, `hsla(${(score.visual.hue * 360 + 40) % 360}, 88%, 10%, 1)`);
  grad.addColorStop(1, `hsla(${(score.visual.hue * 360 + 200) % 360}, 92%, 7%, 1)`);
  ctx.fillStyle = grad;
  ctx.fillRect(0, 0, width, height);

  const cellWidth = width / COLS;
  const rowHeight = height / ROWS;
  ctx.strokeStyle = "rgba(255,255,255,0.08)";
  for (let col = 0; col < COLS; col += 1) {
    ctx.strokeRect(col * cellWidth, 0, cellWidth, height);
  }
  ctx.fillStyle = "rgba(255,255,255,0.06)";
  ctx.fillRect(activeStep * cellWidth, 0, cellWidth, height);

  for (const line of score.visual.lines) {
    const y = rowHeight * (line.row + 0.5);
    const hue = ((line.hue ?? 0.58) * 360 + line.row * 22) % 360;
    ctx.strokeStyle = `hsla(${hue}, 100%, 70%, ${0.2 + line.energy * 0.6})`;
    ctx.lineWidth = 2 + line.energy * 10;
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(width, y + Math.sin(state.currentTime * 4 + line.row) * 10);
    ctx.stroke();

    ctx.fillStyle = `hsla(${(hue + 80) % 360}, 100%, 72%, ${0.18 + line.energy * 0.5})`;
    ctx.beginPath();
    ctx.arc((line.step + 0.5) * cellWidth, y, 8 + line.energy * 40, 0, TAU);
    ctx.fill();
  }
}

function createAudioEngine() {
  const AudioContextCtor = window.AudioContext || window.webkitAudioContext;
  const context = new AudioContextCtor();
  const processor = context.createScriptProcessor(512, 0, 2);
  const gain = context.createGain();
  gain.gain.value = 0;
  processor.connect(gain);
  gain.connect(context.destination);

  processor.onaudioprocess = (event) => {
    const left = event.outputBuffer.getChannelData(0);
    const right = event.outputBuffer.getChannelData(1);
    for (let i = 0; i < left.length; i += 1) {
      const t = state.currentTime + i / context.sampleRate;
      const sample = evaluateScore(t).audio;
      left[i] = sample;
      right[i] = sample;
    }
  };

  return {
    async start() {
      if (context.state === "suspended") await context.resume();
      gain.gain.setTargetAtTime(0.8, context.currentTime, 0.02);
    },
    stop() {
      gain.gain.setTargetAtTime(0, context.currentTime, 0.03);
    },
  };
}

function evaluateExpr(expression, scope) {
  try {
    // Small local expression runtime for the prototype.
    return Function(...Object.keys(scope), `return (${expression});`)(...Object.values(scope));
  } catch {
    return 0;
  }
}

function getCurrentStep(time = state.currentTime) {
  return Math.floor(time * (state.bpm / 60) * 2) % COLS;
}

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

function fract(value) {
  return value - Math.floor(value);
}

function escapeHtml(text) {
  return String(text)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;");
}
