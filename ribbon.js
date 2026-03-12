const TAU = Math.PI * 2;

const processorTypes = {
  source: { label: "source", color: "#00e5ff", defaultCode: "220 + lane * 70" },
  gate: { label: "gate", color: "#ffd449", defaultCode: "beat % 1 < 0.18 ? 1 : 0" },
  drive: { label: "drive", color: "#ff5ca8", defaultCode: "1.6" },
  color: { label: "color", color: "#bb86ff", defaultCode: "0.55 + lane * 0.1" },
  byte: { label: "byte", color: "#ff8a3d", defaultCode: "((tt * ((tt >> 5) | (tt >> 8))) & 63) / 31.5 - 1" },
  audio: { label: "audio", color: "#00ffd5", defaultCode: "1" },
  visual: { label: "visual", color: "#ff67d9", defaultCode: "1" },
};

const state = {
  bpm: 112,
  currentTime: 0,
  running: false,
  selectedLaneId: "lane-1",
  selectedProcessorId: "lane-1:source-1",
  selectedTool: "source",
  lanes: createDefaultLanes(),
  visualState: { hue: 0.58, energy: 0, ribbons: [] },
};

const dom = {
  palette: document.querySelector("#processor-palette"),
  ribbonSvg: document.querySelector("#ribbon-svg"),
  inspectorTitle: document.querySelector("#inspector-title"),
  inspectorBody: document.querySelector("#inspector-body"),
  playToggle: document.querySelector("#play-toggle"),
  resetButton: document.querySelector("#reset-button"),
  bpmReadout: document.querySelector("#bpm-readout"),
  beatReadout: document.querySelector("#beat-readout"),
  audioReadout: document.querySelector("#audio-readout"),
  laneReadout: document.querySelector("#lane-readout"),
  laneReadoutBottom: document.querySelector("#lane-readout-bottom"),
  fpsChip: document.querySelector("#fps-chip"),
  stage: document.querySelector("#stage"),
};

const ctx = dom.stage.getContext("2d");
let audioEngine = null;
let dragState = null;
let lastFrame = performance.now();
let fpsFrames = 0;
let fpsStart = performance.now();

bootstrap();

function bootstrap() {
  renderPalette();
  renderRibbonScene();
  renderInspector();
  bindEvents();
  requestAnimationFrame(frame);
}

function createDefaultLanes() {
  return [
    {
      id: "lane-1",
      y: 170,
      processors: [
        makeProcessor("lane-1", "source", 180, "220"),
        makeProcessor("lane-1", "gate", 390, "beat % 1 < 0.22 ? 1 : 0"),
        makeProcessor("lane-1", "drive", 620, "1.8"),
        makeProcessor("lane-1", "color", 850, "0.56"),
        makeProcessor("lane-1", "audio", 1030, "1"),
        makeProcessor("lane-1", "visual", 1080, "1"),
      ],
    },
    {
      id: "lane-2",
      y: 360,
      processors: [
        makeProcessor("lane-2", "byte", 240, "((tt * ((tt >> 4) | (tt >> 7))) & 63) / 31.5 - 1"),
        makeProcessor("lane-2", "drive", 520, "1.25"),
        makeProcessor("lane-2", "color", 760, "0.84"),
        makeProcessor("lane-2", "audio", 1030, "0.42"),
      ],
    },
    {
      id: "lane-3",
      y: 550,
      processors: [
        makeProcessor("lane-3", "source", 200, "110"),
        makeProcessor("lane-3", "gate", 420, "beat % 0.5 < 0.16 ? 1 : 0"),
        makeProcessor("lane-3", "color", 720, "0.18"),
        makeProcessor("lane-3", "visual", 1020, "1"),
      ],
    },
  ];
}

function makeProcessor(laneId, type, x, code) {
  const index = Math.floor(Math.random() * 100000);
  return {
    id: `${laneId}:${type}-${index}`,
    type,
    x,
    code: code ?? processorTypes[type].defaultCode,
    label: processorTypes[type].label,
  };
}

function renderPalette() {
  dom.palette.innerHTML = Object.entries(processorTypes)
    .map(
      ([key, value]) => `
        <button class="tool-button ${state.selectedTool === key ? "active" : ""}" data-tool="${key}" type="button">
          ${value.label}
        </button>
      `,
    )
    .join("");

  dom.palette.querySelectorAll("[data-tool]").forEach((button) => {
    button.addEventListener("click", () => {
      state.selectedTool = button.dataset.tool;
      renderPalette();
    });
  });
}

function renderRibbonScene() {
  dom.laneReadout.textContent = `Lane ${getSelectedLaneIndex() + 1}`;
  dom.laneReadoutBottom.textContent = String(getSelectedLaneIndex() + 1);

  dom.ribbonSvg.innerHTML = `
    <defs>
      <filter id="glow"><feGaussianBlur stdDeviation="8" result="blur"/><feMerge><feMergeNode in="blur"/><feMergeNode in="SourceGraphic"/></feMerge></filter>
    </defs>
    ${state.lanes.map(renderLane).join("")}
  `;

  dom.ribbonSvg.querySelectorAll("[data-lane-id]").forEach((element) => {
    element.addEventListener("click", () => {
      state.selectedLaneId = element.dataset.laneId;
      renderRibbonScene();
      renderInspector();
    });
  });
  dom.ribbonSvg.querySelectorAll("[data-processor-id]").forEach((element) => {
    element.addEventListener("click", (event) => {
      event.stopPropagation();
      state.selectedProcessorId = element.dataset.processorId;
      state.selectedLaneId = element.dataset.laneId;
      renderRibbonScene();
      renderInspector();
    });
    element.addEventListener("pointerdown", onProcessorPointerDown);
  });
}

function renderLane(lane, index) {
  const selected = lane.id === state.selectedLaneId;
  const baseHue = 190 + index * 70;
  const path = `M 80 ${lane.y} C 240 ${lane.y - 34}, 420 ${lane.y + 34}, 600 ${lane.y} S 940 ${lane.y - 24}, 1120 ${lane.y}`;
  return `
    <g data-lane-id="${lane.id}">
      <path d="${path}" stroke="hsla(${baseHue}, 100%, 70%, ${selected ? 0.72 : 0.34})" stroke-width="${selected ? 16 : 12}" fill="none" stroke-linecap="round" filter="url(#glow)"></path>
      <path d="${path}" stroke="rgba(255,255,255,0.16)" stroke-width="2" fill="none" stroke-linecap="round"></path>
      ${lane.processors
        .sort((a, b) => a.x - b.x)
        .map((processor) => renderProcessor(lane, processor))
        .join("")}
    </g>
  `;
}

function renderProcessor(lane, processor) {
  const selected = processor.id === state.selectedProcessorId;
  const type = processorTypes[processor.type];
  return `
    <g data-processor-id="${processor.id}" data-lane-id="${lane.id}" transform="translate(${processor.x}, ${lane.y})" style="cursor: grab;">
      <rect x="-52" y="-28" width="104" height="56" rx="14" fill="${hexToRgba(type.color, selected ? 0.34 : 0.18)}" stroke="${selected ? "rgba(255,255,255,0.48)" : "rgba(255,255,255,0.18)"}" />
      <text x="0" y="-4" fill="white" text-anchor="middle" font-size="12" font-family="Avenir Next, Segoe UI, sans-serif">${type.label}</text>
      <text x="0" y="14" fill="rgba(255,255,255,0.7)" text-anchor="middle" font-size="11" font-family="SF Mono, Menlo, monospace">${escapeSvg(processor.code.slice(0, 14))}</text>
    </g>
  `;
}

function bindEvents() {
  dom.playToggle.addEventListener("click", togglePlayback);
  dom.resetButton.addEventListener("click", () => {
    state.lanes = createDefaultLanes();
    state.selectedLaneId = "lane-1";
    state.selectedProcessorId = state.lanes[0].processors[0].id;
    renderRibbonScene();
    renderInspector();
  });

  dom.ribbonSvg.addEventListener("dblclick", () => {
    const lane = state.lanes.find((entry) => entry.id === state.selectedLaneId);
    if (!lane) return;
    const processor = makeProcessor(lane.id, state.selectedTool, 640, processorTypes[state.selectedTool].defaultCode);
    lane.processors.push(processor);
    state.selectedProcessorId = processor.id;
    renderRibbonScene();
    renderInspector();
  });
}

function onProcessorPointerDown(event) {
  const processorId = event.currentTarget.dataset.processorId;
  const laneId = event.currentTarget.dataset.laneId;
  dragState = { processorId, laneId, originX: event.clientX };
  window.addEventListener("pointermove", onPointerMove);
  window.addEventListener("pointerup", onPointerUp, { once: true });
}

function onPointerMove(event) {
  if (!dragState) return;
  const lane = state.lanes.find((entry) => entry.id === dragState.laneId);
  const processor = lane?.processors.find((entry) => entry.id === dragState.processorId);
  if (!lane || !processor) return;
  processor.x = clamp(processor.x + (event.clientX - dragState.originX), 120, 1080);
  dragState.originX = event.clientX;
  renderRibbonScene();
}

function onPointerUp() {
  dragState = null;
  window.removeEventListener("pointermove", onPointerMove);
}

function renderInspector() {
  const processor = findSelectedProcessor();
  if (!processor) {
    dom.inspectorTitle.textContent = "Select a processor";
    dom.inspectorBody.className = "inspector-body empty-state";
    dom.inspectorBody.textContent = "Select a processor or double-click the lane canvas to add one with the current tool.";
    return;
  }

  dom.inspectorTitle.textContent = processorTypes[processor.type].label;
  dom.inspectorBody.className = "inspector-body";
  dom.inspectorBody.innerHTML = `
    <div class="field">
      <label>Lane</label>
      <input value="${getSelectedLaneIndex() + 1}" disabled />
    </div>
    <div class="field">
      <label>Type</label>
      <input value="${processor.type}" disabled />
    </div>
    <div class="field">
      <label>Expression</label>
      <textarea id="processor-code">${processor.code}</textarea>
    </div>
    <div class="field">
      <label>Delete</label>
      <button id="delete-processor" class="transport-button secondary" type="button">Delete Processor</button>
    </div>
  `;

  dom.inspectorBody.querySelector("#processor-code").addEventListener("change", (event) => {
    processor.code = event.currentTarget.value;
    renderRibbonScene();
  });
  dom.inspectorBody.querySelector("#delete-processor").addEventListener("click", () => {
    const lane = state.lanes.find((entry) => entry.id === state.selectedLaneId);
    lane.processors = lane.processors.filter((entry) => entry.id !== processor.id);
    state.selectedProcessorId = lane.processors[0]?.id ?? null;
    renderRibbonScene();
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
  dom.beatReadout.textContent = (state.currentTime * (state.bpm / 60)).toFixed(2);

  fpsFrames += 1;
  if (now - fpsStart > 500) {
    dom.fpsChip.textContent = `FPS ${Math.round((fpsFrames * 1000) / (now - fpsStart))}`;
    fpsFrames = 0;
    fpsStart = now;
  }

  state.visualState = evaluateLanes(state.currentTime).visual;
  renderStage();
  lastFrame = now;
  requestAnimationFrame(frame);
}

function evaluateLanes(t) {
  let audio = 0;
  let hue = 0.58;
  const ribbons = [];
  const beat = t * (state.bpm / 60);

  for (const [laneIndex, lane] of state.lanes.entries()) {
    let x = 0;
    let laneHue = hue;
    let laneAudio = 0;
    let laneVisual = 0;
    const ordered = [...lane.processors].sort((a, b) => a.x - b.x);
    for (const processor of ordered) {
      const scope = {
        x,
        t,
        beat,
        lane: laneIndex,
        tt: Math.floor(t * 8000),
        TAU,
      };
      const value = evaluateExpr(processor.code, scope);
      switch (processor.type) {
        case "source":
          x = Math.sin(t * TAU * Math.max(1, value)) * 0.4;
          break;
        case "gate":
          x *= value;
          break;
        case "drive":
          x = Math.tanh(x * value);
          break;
        case "color":
          laneHue = value;
          break;
        case "byte":
          x = value * 0.28;
          break;
        case "audio":
          laneAudio += x * value;
          break;
        case "visual":
          laneVisual += Math.abs(x) * value;
          break;
        default:
          break;
      }
    }
    hue = laneHue;
    audio += laneAudio;
    ribbons.push({ lane: laneIndex, hue: laneHue, energy: laneVisual + Math.abs(laneAudio) });
  }

  return {
    audio: clamp(audio, -1, 1),
    visual: { hue, energy: Math.abs(audio), ribbons },
  };
}

function renderStage() {
  const { width, height } = dom.stage;
  const gradient = ctx.createLinearGradient(0, 0, width, height);
  gradient.addColorStop(0, `hsla(${(state.visualState.hue * 360 + 20) % 360}, 84%, 12%, 1)`);
  gradient.addColorStop(1, `hsla(${(state.visualState.hue * 360 + 180) % 360}, 86%, 8%, 1)`);
  ctx.fillStyle = gradient;
  ctx.fillRect(0, 0, width, height);

  for (const ribbon of state.visualState.ribbons) {
    const y = 90 + ribbon.lane * 90;
    const hue = ((ribbon.hue ?? 0.5) * 360 + ribbon.lane * 40) % 360;
    ctx.strokeStyle = `hsla(${hue}, 100%, 70%, ${0.18 + ribbon.energy * 0.5})`;
    ctx.lineWidth = 10 + ribbon.energy * 24;
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.bezierCurveTo(150, y - 24, 300, y + 20, 420, y);
    ctx.bezierCurveTo(540, y - 20, 700, y + 26, 860, y);
    ctx.bezierCurveTo(980, y - 24, 1080, y + 14, width, y);
    ctx.stroke();
  }

  ctx.fillStyle = `hsla(${(state.visualState.hue * 360 + 90) % 360}, 100%, 72%, 0.26)`;
  ctx.beginPath();
  ctx.arc(width * 0.5, height * 0.5, 34 + state.visualState.energy * 120, 0, TAU);
  ctx.fill();
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
      const sample = evaluateLanes(t).audio;
      left[i] = sample;
      right[i] = sample;
    }
  };

  return {
    async start() {
      if (context.state === "suspended") await context.resume();
      gain.gain.setTargetAtTime(0.85, context.currentTime, 0.02);
    },
    stop() {
      gain.gain.setTargetAtTime(0, context.currentTime, 0.03);
    },
  };
}

function findSelectedProcessor() {
  return state.lanes.flatMap((lane) => lane.processors).find((entry) => entry.id === state.selectedProcessorId) ?? null;
}

function getSelectedLaneIndex() {
  return Math.max(0, state.lanes.findIndex((entry) => entry.id === state.selectedLaneId));
}

function evaluateExpr(expression, scope) {
  try {
    return Function(...Object.keys(scope), `return (${expression});`)(...Object.values(scope));
  } catch {
    return 0;
  }
}

function hexToRgba(hex, alpha) {
  const int = Number.parseInt(hex.slice(1), 16);
  return `rgba(${(int >> 16) & 255}, ${(int >> 8) & 255}, ${int & 255}, ${alpha})`;
}

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

function escapeSvg(text) {
  return String(text)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;");
}
