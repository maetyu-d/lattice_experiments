const TAU = Math.PI * 2;

const nodeTemplates = {
  clock: {
    title: "Clock",
    subtitle: "transport",
    width: 220,
    accent: "clock",
    inputs: [],
    outputs: [{ id: "pulse", label: "pulse", type: "trig" }],
    paramDefs: {
      bpm: { min: 30, max: 220, step: 1, default: 120, type: "sig" },
    },
    code: "pulse = fract(time * (bpm / 60)) < 0.03 ? 1 : 0",
  },
  lfo: {
    title: "LFO",
    subtitle: "modulator",
    width: 228,
    accent: "lfo",
    inputs: [],
    outputs: [{ id: "value", label: "value", type: "sig" }],
    paramDefs: {
      rate: { min: 0, max: 8, step: 0.01, default: 0.38, type: "sig" },
      depth: { min: 0, max: 1, step: 0.01, default: 0.24, type: "sig" },
      offset: { min: 0, max: 1, step: 0.01, default: 0.44, type: "sig" },
      phase: { min: 0, max: 1, step: 0.01, default: 0, type: "sig" },
    },
    code: "value = offset + sin((time + phase) * TAU * rate) * depth",
  },
  osc: {
    title: "Osc",
    subtitle: "audio source",
    width: 244,
    accent: "osc",
    inputs: [{ id: "fm", label: "fm", type: "sig" }],
    outputs: [
      { id: "audio", label: "audio", type: "sig" },
      { id: "energy", label: "energy", type: "sig" },
    ],
    paramDefs: {
      freq: { min: 20, max: 1600, step: 1, default: 220, type: "sig" },
      gain: { min: 0, max: 1, step: 0.01, default: 0.18, type: "sig" },
      bend: { min: 0, max: 1200, step: 1, default: 120, type: "sig" },
    },
    code: "sine oscillator with fm input and gain",
  },
  noise: {
    title: "Noise",
    subtitle: "audio source",
    width: 232,
    accent: "noise",
    inputs: [],
    outputs: [
      { id: "audio", label: "audio", type: "sig" },
      { id: "energy", label: "energy", type: "sig" },
    ],
    paramDefs: {
      gain: { min: 0, max: 1, step: 0.01, default: 0.08, type: "sig" },
      rate: { min: 200, max: 16000, step: 10, default: 4000, type: "sig" },
    },
    code: "sampled noise source with rate-dependent sparkle",
  },
  env: {
    title: "Env",
    subtitle: "modulator",
    width: 236,
    accent: "env",
    inputs: [{ id: "trig", label: "trig", type: "trig" }],
    outputs: [
      { id: "env", label: "env", type: "sig" },
      { id: "energy", label: "energy", type: "sig" },
    ],
    paramDefs: {
      attack: { min: 0.001, max: 1.5, step: 0.001, default: 0.02, type: "sig" },
      release: { min: 0.005, max: 3, step: 0.001, default: 0.32, type: "sig" },
      amount: { min: 0, max: 1.5, step: 0.01, default: 1, type: "sig" },
    },
    code: "triggered attack/release envelope",
  },
  filter: {
    title: "Filter",
    subtitle: "audio shaper",
    width: 252,
    accent: "filter",
    inputs: [{ id: "audio", label: "audio", type: "sig" }],
    outputs: [
      { id: "audio", label: "audio", type: "sig" },
      { id: "energy", label: "energy", type: "sig" },
    ],
    paramDefs: {
      cutoff: { min: 20, max: 12000, step: 1, default: 1400, type: "sig" },
      resonance: { min: 0, max: 0.98, step: 0.01, default: 0.18, type: "sig" },
      drive: { min: 0.5, max: 3, step: 0.01, default: 1, type: "sig" },
    },
    code: "stateful low-pass filter with resonance and drive",
  },
  amp: {
    title: "Amp",
    subtitle: "audio shaper",
    width: 244,
    accent: "amp",
    inputs: [
      { id: "audio", label: "audio", type: "sig" },
      { id: "cv", label: "cv", type: "sig" },
    ],
    outputs: [
      { id: "audio", label: "audio", type: "sig" },
      { id: "energy", label: "energy", type: "sig" },
    ],
    paramDefs: {
      gain: { min: 0, max: 2, step: 0.01, default: 0.8, type: "sig" },
      bias: { min: -1, max: 1, step: 0.01, default: 0, type: "sig" },
    },
    code: "scales incoming audio with gain + cv and soft saturation",
  },
  mixer: {
    title: "Mixer",
    subtitle: "audio utility",
    width: 250,
    accent: "mixer",
    inputs: [
      { id: "a", label: "a", type: "sig" },
      { id: "b", label: "b", type: "sig" },
    ],
    outputs: [
      { id: "audio", label: "audio", type: "sig" },
      { id: "energy", label: "energy", type: "sig" },
    ],
    paramDefs: {
      aGain: { min: 0, max: 1.5, step: 0.01, default: 1, type: "sig" },
      bGain: { min: 0, max: 1.5, step: 0.01, default: 1, type: "sig" },
      trim: { min: 0, max: 1, step: 0.01, default: 0.7, type: "sig" },
    },
    code: "mixes two audio signals into one output bus",
  },
  bytebeat: {
    title: "ByteLead",
    subtitle: "code node",
    width: 264,
    accent: "bytebeat",
    inputs: [{ id: "pulse", label: "pulse", type: "trig" }],
    outputs: [
      { id: "audio", label: "audio", type: "sig" },
      { id: "energy", label: "energy", type: "sig" },
    ],
    paramDefs: {
      rate: { min: 4000, max: 16000, step: 1, default: 8000, type: "sig" },
      gain: { min: 0, max: 1, step: 0.01, default: 0.14, type: "sig" },
      tone: { min: 1, max: 8, step: 1, default: 5, type: "sig" },
    },
    code:
      "const tt = Math.floor(t * rate);\n" +
      "const x = (tt * ((tt >> Math.floor(tone)) | (tt >> 8))) & 63;\n" +
      "const raw = ((x / 31.5) - 1) * gain;\n" +
      "return { audio: raw, energy: Math.abs(raw) };",
  },
  visual: {
    title: "MinimalViz",
    subtitle: "visual node",
    width: 264,
    accent: "visual",
    inputs: [
      { id: "energy", label: "energy", type: "sig" },
      { id: "pulse", label: "pulse", type: "trig" },
    ],
    outputs: [{ id: "image", label: "image", type: "img" }],
    paramDefs: {
      hue: { min: 0, max: 1, step: 0.01, default: 0.58, type: "sig" },
      spin: { min: 0, max: 2, step: 0.01, default: 0.35, type: "sig" },
      scale: { min: 0.2, max: 1.8, step: 0.01, default: 0.92, type: "sig" },
    },
    code:
      "const radius = 34 + energy * 156 * scale + flash * 28;\n" +
      "const arm = 84 + flash * 118 + energy * 70;\n" +
      "return { radius, angle: t * spin * TAU, arm, hue, flash };",
  },
  output: {
    title: "Output",
    subtitle: "sink",
    width: 200,
    accent: "output",
    inputs: [
      { id: "audio", label: "audio", type: "sig" },
      { id: "image", label: "image", type: "img" },
    ],
    outputs: [],
    paramDefs: {},
    code: "routes audio + image to the main outputs",
  },
};

const state = {
  nodes: [
    { id: "clock-1", type: "clock", x: 42, y: 56 },
    { id: "env-1", type: "env", x: 288, y: 42 },
    { id: "osc-1", type: "osc", x: 544, y: 40 },
    { id: "filter-1", type: "filter", x: 814, y: 40 },
    { id: "amp-1", type: "amp", x: 1088, y: 40 },
    { id: "lfo-1", type: "lfo", x: 288, y: 286 },
    { id: "bytebeat-1", type: "bytebeat", x: 546, y: 286 },
    { id: "mixer-1", type: "mixer", x: 1088, y: 272 },
    { id: "visual-1", type: "visual", x: 814, y: 318 },
    { id: "output-1", type: "output", x: 1360, y: 166 },
  ].map(hydrateNode),
  connections: [
    makeConnection("clock-1", "output", "pulse", "env-1", "input", "trig", "trig"),
    makeConnection("clock-1", "output", "pulse", "bytebeat-1", "input", "pulse", "trig"),
    makeConnection("clock-1", "output", "pulse", "visual-1", "input", "pulse", "trig"),
    makeConnection("env-1", "output", "env", "amp-1", "input", "cv", "sig"),
    makeConnection("env-1", "output", "env", "osc-1", "param", "gain", "sig"),
    makeConnection("lfo-1", "output", "value", "osc-1", "input", "fm", "sig"),
    makeConnection("lfo-1", "output", "value", "filter-1", "param", "cutoff", "sig"),
    makeConnection("lfo-1", "output", "value", "visual-1", "param", "hue", "sig"),
    makeConnection("osc-1", "output", "audio", "filter-1", "input", "audio", "sig"),
    makeConnection("filter-1", "output", "audio", "amp-1", "input", "audio", "sig"),
    makeConnection("amp-1", "output", "audio", "mixer-1", "input", "a", "sig"),
    makeConnection("amp-1", "output", "energy", "visual-1", "input", "energy", "sig"),
    makeConnection("bytebeat-1", "output", "audio", "mixer-1", "input", "b", "sig"),
    makeConnection("bytebeat-1", "output", "energy", "visual-1", "param", "scale", "sig"),
    makeConnection("mixer-1", "output", "audio", "output-1", "input", "audio", "sig"),
    makeConnection("visual-1", "output", "image", "output-1", "input", "image", "img"),
  ],
  selectedNodeId: "bytebeat-1",
  pendingCable: null,
  pointer: { x: 0, y: 0 },
  transport: {
    startedAt: 0,
    currentTime: 0,
    beat: 0,
    running: false,
    flash: 0,
  },
  runtime: {
    compiled: {},
    lastPulseHigh: false,
    latestImage: null,
    latestAudio: 0,
    latestEnergy: 0,
    nodeState: {},
  },
};

const dom = {
  canvas: document.querySelector("#patch-canvas"),
  cableLayer: document.querySelector("#cable-layer"),
  inspectorTitle: document.querySelector("#inspector-title"),
  inspectorBody: document.querySelector("#inspector-body"),
  stage: document.querySelector("#stage"),
  playToggle: document.querySelector("#play-toggle"),
  resetButton: document.querySelector("#reset-button"),
  bpmReadout: document.querySelector("#bpm-readout"),
  beatReadout: document.querySelector("#beat-readout"),
  cpuReadout: document.querySelector("#cpu-readout"),
  audioReadout: document.querySelector("#audio-readout"),
  fpsChip: document.querySelector("#fps-chip"),
  connectionStatus: document.querySelector("#connection-status"),
};

const stageContext = dom.stage.getContext("2d");
const nodeElements = new Map();

let audioEngine = null;
let dragState = null;
let lastFrame = performance.now();
let fpsSampleStart = performance.now();
let fpsFrames = 0;

bootstrap();

function bootstrap() {
  compileRuntime();
  renderNodes();
  renderConnections();
  renderInspector();
  syncTransportReadout();
  bindGlobalEvents();
  requestAnimationFrame(frame);
}

function hydrateNode(node) {
  const template = nodeTemplates[node.type];
  const params = {};
  for (const [name, definition] of Object.entries(template.paramDefs)) {
    params[name] = node.params?.[name] ?? definition.default;
  }
  return {
    ...node,
    width: template.width,
    params,
    code: node.code ?? template.code,
  };
}

function makeConnection(fromNodeId, fromKind, fromPortId, toNodeId, toKind, toPortId, type) {
  return {
    id: `${fromNodeId}:${fromKind}:${fromPortId}->${toNodeId}:${toKind}:${toPortId}`,
    from: { nodeId: fromNodeId, kind: fromKind, portId: fromPortId },
    to: { nodeId: toNodeId, kind: toKind, portId: toPortId },
    type,
  };
}

function compileRuntime() {
  state.runtime.compiled = {};
  for (const node of state.nodes) {
    if (node.type === "bytebeat" || node.type === "visual") {
      const paramNames = Object.keys(node.params);
      const inputNames = nodeTemplates[node.type].inputs.map((input) => input.id);
      state.runtime.compiled[node.id] = new Function(
        "t",
        "inputs",
        "params",
        "context",
        [
          `const { ${inputNames.join(", ")} } = inputs;`,
          `const { ${paramNames.join(", ")} } = params;`,
          "const { TAU, flash } = context;",
          `${node.code}`,
        ].join("\n"),
      );
    }
  }
}

function renderNodes() {
  dom.canvas.querySelectorAll(".node").forEach((node) => node.remove());
  nodeElements.clear();

  for (const node of state.nodes) {
    const template = nodeTemplates[node.type];
    const leftItems = [
      ...template.inputs.map((port) => ({ ...port, kind: "input" })),
      ...Object.entries(template.paramDefs).map(([id, def]) => ({
        id,
        label: id,
        type: def.type,
        kind: "param",
      })),
    ];
    const rightItems = template.outputs.map((port) => ({ ...port, kind: "output" }));
    const maxRows = Math.max(leftItems.length, rightItems.length, 1);
    const preview = node.type === "output" ? template.code : node.code;

    const element = document.createElement("article");
    element.className = `node ${template.accent}${state.selectedNodeId === node.id ? " selected" : ""}`;
    element.style.width = `${node.width}px`;
    element.style.transform = `translate(${node.x}px, ${node.y}px)`;
    element.dataset.nodeId = node.id;
    element.innerHTML = `
      <div class="node-header" data-drag-handle="true">
        <div class="node-title">
          <strong>${template.title}</strong>
          <span>${template.subtitle}</span>
        </div>
        <span class="status-chip">${node.id}</span>
      </div>
      <div class="node-body">
        <div class="node-preview">${escapeHtml(preview)}</div>
        <div class="port-list">
          ${Array.from({ length: maxRows }, (_, index) =>
            renderPortRow(node, leftItems[index], rightItems[index]),
          ).join("")}
        </div>
      </div>
    `;

    element.addEventListener("pointerdown", (event) => onNodePointerDown(event, node.id));
    element.addEventListener("click", () => selectNode(node.id));
    nodeElements.set(node.id, element);
    dom.canvas.appendChild(element);
  }

  dom.canvas.querySelectorAll("[data-port-key]").forEach((portButton) => {
    portButton.addEventListener("click", onPortClick);
  });
}

function renderPortRow(node, leftItem, rightItem) {
  return `
    <div class="port-row">
      ${renderPortSide(node, leftItem, "left")}
      ${renderPortSide(node, rightItem, "right")}
    </div>
  `;
}

function renderPortSide(node, item, side) {
  if (!item) {
    return `<div class="port-side ${side}"></div>`;
  }
  const key = makePortKey(node.id, item.kind, item.id);
  const isPending = state.pendingCable?.fromKey === key;
  const modulationCount =
    item.kind === "param"
      ? state.connections.filter(
          (connection) =>
            connection.to.nodeId === node.id &&
            connection.to.kind === "param" &&
            connection.to.portId === item.id,
        ).length
      : 0;
  const valueLabel =
    item.kind === "param" ? `<span class="port-value">${formatParam(node.params[item.id])}</span>` : "";
  const modLabel =
    item.kind === "param" && modulationCount > 0
      ? `<span class="port-mod-count">+${modulationCount}</span>`
      : "";

  return `
    <div class="port-side ${side}">
      <button
        class="port-hit ${item.type} ${item.kind} ${isPending ? "pending" : ""}"
        data-port-key="${key}"
        data-node-id="${node.id}"
        data-port-id="${item.id}"
        data-port-kind="${item.kind}"
        data-port-type="${item.type}"
        data-direction="${side === "right" ? "out" : "in"}"
        type="button"
        title="${side === "right" ? "Click then choose a target port" : "Click to patch here. Shift-click to clear"}"
      >
        ${
          side === "left"
            ? `<span class="port ${item.type}"></span><span class="port-label">${item.kind === "param" ? `mod ${item.label}` : item.label}</span>${valueLabel}${modLabel}`
            : `<span class="port-label">${item.label}</span><span class="port ${item.type}"></span>`
        }
      </button>
    </div>
  `;
}

function makePortKey(nodeId, kind, portId) {
  return `${nodeId}:${kind}:${portId}`;
}

function renderConnections() {
  dom.cableLayer.innerHTML = "";
  const connectionCount = state.connections.length + (state.pendingCable ? 1 : 0);
  dom.connectionStatus.textContent = state.pendingCable
    ? `Patch from ${state.pendingCable.from.portId} -> choose an input or param`
    : `${state.nodes.length} nodes / ${state.connections.length} cables`;

  const canvasRect = dom.canvas.getBoundingClientRect();
  for (const connection of state.connections) {
    renderCable(connection.from, connection.to, connection.type, canvasRect, false);
  }

  if (state.pendingCable) {
    renderCable(
      state.pendingCable.from,
      { point: state.pointer },
      state.pendingCable.type,
      canvasRect,
      true,
    );
  }

  dom.cableLayer.dataset.count = String(connectionCount);
}

function renderCable(from, to, type, canvasRect, preview) {
  const start = getAnchorPosition(from, canvasRect, true);
  const end = getAnchorPosition(to, canvasRect, false);
  if (!start || !end) return;

  const dx = Math.max(90, Math.abs(end.x - start.x) * 0.42);
  const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
  path.setAttribute(
    "d",
    `M ${start.x} ${start.y} C ${start.x + dx} ${start.y}, ${end.x - dx} ${end.y}, ${end.x} ${end.y}`,
  );
  path.setAttribute("class", `cable-path ${type}${preview ? " preview" : ""}`);
  dom.cableLayer.appendChild(path);
}

function getAnchorPosition(endpoint, canvasRect, outputSide) {
  if (endpoint.point) {
    return {
      x: endpoint.point.x - canvasRect.left,
      y: endpoint.point.y - canvasRect.top,
    };
  }

  const portEl = resolvePortAnchor(endpoint);
  if (!portEl) return null;
  const rect = portEl.getBoundingClientRect();
  return {
    x: rect.left - canvasRect.left + rect.width / 2 + (outputSide ? 3 : -3),
    y: rect.top - canvasRect.top + rect.height / 2,
  };
}

function resolvePortAnchor(endpoint) {
  const nodeEl = nodeElements.get(endpoint.nodeId);
  if (!nodeEl) return null;
  return nodeEl.querySelector(`[data-port-key="${makePortKey(endpoint.nodeId, endpoint.kind, endpoint.portId)}"] .port`);
}

function renderInspector() {
  const node = state.nodes.find((item) => item.id === state.selectedNodeId);
  if (!node) {
    dom.inspectorTitle.textContent = "Select a node";
    dom.inspectorBody.className = "inspector-body empty-state";
    dom.inspectorBody.textContent = "Click a node to edit code. Click an output port, then an input or param port to patch modulation.";
    return;
  }

  const template = nodeTemplates[node.type];
  dom.inspectorTitle.textContent = template.title;
  dom.inspectorBody.className = "inspector-body";
  dom.inspectorBody.innerHTML = `
    <section class="inspector-section">
      <div class="section-label">Metadata</div>
      <div class="meta-grid">
        <div class="meta-row">
          <label>Name</label>
          <input value="${node.id}" disabled />
        </div>
        <div class="meta-row">
          <label>Type</label>
          <input value="${template.subtitle}" disabled />
        </div>
      </div>
    </section>
    <section class="inspector-section">
      <div class="section-label">Parameters</div>
      <div class="param-grid">
        ${Object.entries(template.paramDefs)
          .map(([name, definition]) => {
            const incomingMods = state.connections.filter(
              (connection) =>
                connection.to.nodeId === node.id &&
                connection.to.kind === "param" &&
                connection.to.portId === name,
            ).length;
            return `
              <div class="param-row">
                <label>${name}${incomingMods ? ` · ${incomingMods} mod` : ""}</label>
                <div class="param-inline">
                  <input
                    type="range"
                    min="${definition.min}"
                    max="${definition.max}"
                    step="${definition.step}"
                    value="${node.params[name]}"
                    data-param-field="${name}"
                  />
                  <output>${formatParam(node.params[name])}</output>
                </div>
              </div>
            `;
          })
          .join("")}
      </div>
    </section>
    <section class="inspector-section">
      <div class="section-label">Code</div>
      <div class="param-row">
        <label>Expression</label>
        <textarea data-code-field="code">${node.code}</textarea>
      </div>
    </section>
  `;

  dom.inspectorBody.querySelectorAll("[data-param-field]").forEach((input) => {
    input.addEventListener("input", (event) => {
      const field = event.currentTarget.dataset.paramField;
      node.params[field] = Number(event.currentTarget.value);
      event.currentTarget.nextElementSibling.textContent = formatParam(node.params[field]);
      if (node.type === "clock" && field === "bpm") {
        dom.bpmReadout.textContent = node.params[field].toFixed(0);
      }
      renderNodes();
      renderConnections();
    });
  });

  dom.inspectorBody.querySelector("[data-code-field]").addEventListener("change", (event) => {
    node.code = event.currentTarget.value;
    compileRuntime();
    renderNodes();
    renderConnections();
  });
}

function bindGlobalEvents() {
  window.addEventListener("resize", renderConnections);
  window.addEventListener("pointermove", onGlobalPointerMove);
  dom.canvas.addEventListener("click", (event) => {
    if (event.target === dom.canvas && state.pendingCable) {
      state.pendingCable = null;
      renderNodes();
      renderConnections();
    }
  });

  document.querySelectorAll(".library-item").forEach((button) => {
    button.addEventListener("click", () => addNode(button.dataset.nodeType));
  });

  dom.playToggle.addEventListener("click", toggleTransport);
  dom.resetButton.addEventListener("click", resetTransport);
}

function onGlobalPointerMove(event) {
  state.pointer = { x: event.clientX, y: event.clientY };
  if (state.pendingCable) {
    renderConnections();
  }
}

function addNode(type) {
  const count = state.nodes.filter((node) => node.type === type).length + 1;
  const node = hydrateNode({
    id: `${type}-${count}`,
    type,
    x: 110 + count * 28,
    y: 110 + count * 28,
  });
  state.nodes.push(node);
  compileRuntime();
  renderNodes();
  renderConnections();
}

function onNodePointerDown(event, nodeId) {
  if (!event.target.closest("[data-drag-handle='true']")) return;
  const node = state.nodes.find((item) => item.id === nodeId);
  if (!node) return;
  selectNode(nodeId);
  dragState = {
    nodeId,
    offsetX: event.clientX - node.x,
    offsetY: event.clientY - node.y,
  };
  window.addEventListener("pointermove", onNodePointerMove);
  window.addEventListener("pointerup", onNodePointerUp, { once: true });
}

function onNodePointerMove(event) {
  if (!dragState) return;
  const node = state.nodes.find((item) => item.id === dragState.nodeId);
  if (!node) return;
  const canvasRect = dom.canvas.getBoundingClientRect();
  node.x = clamp(event.clientX - dragState.offsetX - canvasRect.left, 24, dom.canvas.clientWidth - node.width - 24);
  node.y = clamp(event.clientY - dragState.offsetY - canvasRect.top, 24, dom.canvas.clientHeight - 160);
  const element = nodeElements.get(node.id);
  if (element) {
    element.style.transform = `translate(${node.x}px, ${node.y}px)`;
  }
  renderConnections();
}

function onNodePointerUp() {
  dragState = null;
  window.removeEventListener("pointermove", onNodePointerMove);
}

function onPortClick(event) {
  event.stopPropagation();
  const button = event.currentTarget;
  const port = {
    nodeId: button.dataset.nodeId,
    kind: button.dataset.portKind,
    portId: button.dataset.portId,
    type: button.dataset.portType,
    direction: button.dataset.direction,
  };

  if (event.shiftKey) {
    clearConnectionsForPort(port);
    return;
  }

  if (!state.pendingCable) {
    if (port.direction !== "out") return;
    state.pendingCable = {
      from: { nodeId: port.nodeId, kind: port.kind, portId: port.portId },
      fromKey: makePortKey(port.nodeId, port.kind, port.portId),
      type: port.type,
    };
    renderNodes();
    renderConnections();
    return;
  }

  if (port.direction === "out") {
    state.pendingCable = {
      from: { nodeId: port.nodeId, kind: port.kind, portId: port.portId },
      fromKey: makePortKey(port.nodeId, port.kind, port.portId),
      type: port.type,
    };
    renderNodes();
    renderConnections();
    return;
  }

  if (!canConnect(state.pendingCable.type, port.type, port.kind)) {
    state.pendingCable = null;
    renderNodes();
    renderConnections();
    return;
  }

  const connection = makeConnection(
    state.pendingCable.from.nodeId,
    state.pendingCable.from.kind,
    state.pendingCable.from.portId,
    port.nodeId,
    port.kind,
    port.portId,
    state.pendingCable.type,
  );

  if (!state.connections.some((existing) => existing.id === connection.id)) {
    state.connections.push(connection);
  }
  state.pendingCable = null;
  renderNodes();
  renderConnections();
  renderInspector();
}

function clearConnectionsForPort(port) {
  state.connections = state.connections.filter((connection) => {
    if (port.direction === "out") {
      return !(
        connection.from.nodeId === port.nodeId &&
        connection.from.kind === port.kind &&
        connection.from.portId === port.portId
      );
    }
    return !(
      connection.to.nodeId === port.nodeId &&
      connection.to.kind === port.kind &&
      connection.to.portId === port.portId
    );
  });
  state.pendingCable = null;
  renderNodes();
  renderConnections();
  renderInspector();
}

function canConnect(fromType, toType, toKind) {
  if (toKind === "param") {
    return fromType === "sig" || fromType === "trig";
  }
  return fromType === toType || (fromType === "trig" && toType === "sig");
}

function selectNode(nodeId) {
  state.selectedNodeId = nodeId;
  renderNodes();
  renderConnections();
  renderInspector();
}

function toggleTransport() {
  state.transport.running = !state.transport.running;
  dom.playToggle.textContent = state.transport.running ? "Stop" : "Play";
  dom.audioReadout.textContent = state.transport.running ? "running" : "stopped";

  if (state.transport.running) {
    state.transport.startedAt = performance.now() / 1000 - state.transport.currentTime;
    if (!audioEngine) {
      audioEngine = createAudioEngine();
    }
    audioEngine.start();
  } else if (audioEngine) {
    audioEngine.stop();
  }
}

function resetTransport() {
  state.transport.currentTime = 0;
  state.transport.beat = 0;
  state.transport.flash = 0;
  state.runtime.lastPulseHigh = false;
  state.runtime.nodeState = {};
  syncTransportReadout();
}

function frame(now) {
  if (state.transport.running) {
    state.transport.currentTime = performance.now() / 1000 - state.transport.startedAt;
  }

  fpsFrames += 1;
  if (now - fpsSampleStart >= 500) {
    const fps = Math.round((fpsFrames * 1000) / (now - fpsSampleStart));
    dom.fpsChip.textContent = `FPS ${fps}`;
    fpsSampleStart = now;
    fpsFrames = 0;
  }

  const graph = evaluateGraph(state.transport.currentTime, "frame", Math.max(1 / 240, (now - lastFrame) / 1000));
  state.runtime.latestImage = graph.output?.image ?? null;
  state.runtime.latestAudio = graph.output?.audio ?? 0;
  state.runtime.latestEnergy = graph.bytebeat?.energy ?? 0;
  updateTransportFromGraph(graph.clock);
  renderStage();
  syncTransportReadout();

  lastFrame = now;
  requestAnimationFrame(frame);
}

function evaluateGraph(t, mode, dt = mode === "audio" ? 1 / 44100 : 1 / 60) {
  const cache = new Map();
  const stack = new Set();

  const evaluateNode = (nodeId) => {
    if (cache.has(nodeId)) return cache.get(nodeId);
    if (stack.has(nodeId)) return {};
    const node = state.nodes.find((entry) => entry.id === nodeId);
    if (!node) return {};

    stack.add(nodeId);
    const template = nodeTemplates[node.type];
    const inputs = {};
    for (const input of template.inputs) {
      inputs[input.id] = resolveIncomingValue(node.id, "input", input.id, input.type, evaluateNode);
    }

    const params = {};
    for (const [name, definition] of Object.entries(template.paramDefs)) {
      const modulation = resolveIncomingValue(node.id, "param", name, definition.type, evaluateNode);
      params[name] = clamp(node.params[name] + modulation, definition.min, definition.max);
    }

    let result = {};
    const nodeState = getNodeState(node.id, mode);
    if (node.type === "clock") {
      const beat = t * (params.bpm / 60);
      result = { pulse: fract(beat) < 0.03 ? 1 : 0, beat, bpm: params.bpm };
    } else if (node.type === "lfo") {
      result = {
        value: params.offset + Math.sin((t + params.phase) * TAU * params.rate) * params.depth,
      };
    } else if (node.type === "osc") {
      const hz = clamp(params.freq + inputs.fm * params.bend, 1, 22000);
      const sample = Math.sin(t * TAU * hz) * params.gain;
      result = { audio: sample, energy: Math.abs(sample) };
    } else if (node.type === "noise") {
      const steppedTime = Math.floor(t * params.rate);
      const hash = fract(Math.sin(steppedTime * 12.9898 + 78.233) * 43758.5453);
      const sample = ((hash * 2) - 1) * params.gain;
      result = { audio: sample, energy: Math.abs(sample) };
    } else if (node.type === "env") {
      const trigHigh = (inputs.trig ?? 0) > 0.5;
      const attackCoef = 1 - Math.exp(-dt / Math.max(0.001, params.attack));
      const releaseCoef = 1 - Math.exp(-dt / Math.max(0.001, params.release));
      if (trigHigh) {
        nodeState.value += (1 - nodeState.value) * attackCoef;
      } else {
        nodeState.value += (0 - nodeState.value) * releaseCoef;
      }
      nodeState.value = clamp(nodeState.value, 0, 1.5);
      result = {
        env: nodeState.value * params.amount,
        energy: Math.abs(nodeState.value * params.amount),
      };
    } else if (node.type === "filter") {
      const cutoff = clamp(params.cutoff, 20, 18000);
      const omega = Math.exp((-TAU * cutoff) * dt);
      const driveIn = Math.tanh((inputs.audio ?? 0) * params.drive + nodeState.stage2 * params.resonance);
      nodeState.stage1 = (1 - omega) * driveIn + omega * nodeState.stage1;
      nodeState.stage2 = (1 - omega) * nodeState.stage1 + omega * nodeState.stage2;
      const sample = nodeState.stage2;
      result = { audio: sample, energy: Math.abs(sample) };
    } else if (node.type === "amp") {
      const amount = clamp(params.gain + inputs.cv, 0, 2);
      const sample = Math.tanh((inputs.audio + params.bias) * amount);
      result = { audio: sample, energy: Math.abs(sample) };
    } else if (node.type === "mixer") {
      const sample = (inputs.a * params.aGain + inputs.b * params.bGain) * params.trim;
      result = { audio: sample, energy: Math.abs(sample) };
    } else if (node.type === "bytebeat" || node.type === "visual") {
      try {
        result =
          state.runtime.compiled[node.id]?.(t, inputs, params, {
            TAU,
            flash: mode === "frame" ? state.transport.flash : 0,
            dt,
          }) ?? {};
      } catch (error) {
        console.error(`Node execution failed for ${node.id}`, error);
        result = {};
      }
    } else if (node.type === "output") {
      result = {
        audio: inputs.audio ?? 0,
        image: inputs.image ?? null,
      };
    }

    stack.delete(nodeId);
    cache.set(nodeId, result);
    return result;
  };

  const output = evaluateNode("output-1");
  const clock = evaluateNode("clock-1");
  const bytebeat = evaluateNode("bytebeat-1");
  return { output, clock, bytebeat };
}

function resolveIncomingValue(nodeId, kind, portId, type, evaluateNode) {
  const incoming = state.connections.filter(
    (connection) =>
      connection.to.nodeId === nodeId &&
      connection.to.kind === kind &&
      connection.to.portId === portId,
  );

  if (type === "img") {
    const last = incoming.at(-1);
    return last ? evaluateNode(last.from.nodeId)?.[last.from.portId] ?? null : null;
  }

  if (!incoming.length) return 0;

  let sum = 0;
  for (const connection of incoming) {
    const value = evaluateNode(connection.from.nodeId)?.[connection.from.portId] ?? 0;
    if (type === "trig") {
      sum = Math.max(sum, Number(value));
    } else {
      sum += Number(value);
    }
  }
  return sum;
}

function updateTransportFromGraph(clock) {
  if (!clock) return;
  state.transport.beat = clock.beat ?? 0;
  const pulseHigh = (clock.pulse ?? 0) > 0.5;
  if (pulseHigh && !state.runtime.lastPulseHigh) {
    state.transport.flash = 1;
  }
  state.runtime.lastPulseHigh = pulseHigh;
  state.transport.flash *= 0.9;
}

function renderStage() {
  const width = dom.stage.width;
  const height = dom.stage.height;
  const image = state.runtime.latestImage;
  const energy = state.runtime.latestEnergy;
  const time = state.transport.currentTime;

  const gradient = stageContext.createLinearGradient(0, 0, width, height);
  gradient.addColorStop(0, `hsla(${(time * 40) % 360}, 82%, 14%, 1)`);
  gradient.addColorStop(1, `hsla(${(180 + time * 20) % 360}, 88%, 8%, 1)`);
  stageContext.fillStyle = gradient;
  stageContext.fillRect(0, 0, width, height);

  stageContext.save();
  for (let i = 0; i < 8; i += 1) {
    const x = width * (i / 7);
    stageContext.strokeStyle = `hsla(${(time * 60 + i * 22) % 360}, 100%, 70%, 0.12)`;
    stageContext.beginPath();
    stageContext.moveTo(x, 0);
    stageContext.lineTo(x + Math.sin(time + i) * 10, height);
    stageContext.stroke();
  }
  stageContext.restore();

  if (!image) return;

  const centerX = width * 0.5;
  const centerY = height * 0.5;
  const hue = ((image.hue ?? 0.58) * 360 + time * 55) % 360;
  const radius = image.radius ?? 80;
  const angle = image.angle ?? 0;
  const arm = image.arm ?? 120;
  const flash = image.flash ?? 0;
  const endX = centerX + Math.cos(angle) * arm;
  const endY = centerY + Math.sin(angle) * arm;

  stageContext.save();
  stageContext.shadowBlur = 34 + flash * 22;
  stageContext.shadowColor = `hsla(${hue}, 100%, 70%, 0.9)`;
  stageContext.strokeStyle = `hsla(${hue}, 100%, 72%, 0.96)`;
  stageContext.lineWidth = 2 + energy * 3;
  stageContext.beginPath();
  stageContext.arc(centerX, centerY, radius, 0, TAU);
  stageContext.stroke();

  stageContext.strokeStyle = `hsla(${(hue + 80) % 360}, 100%, 76%, 0.92)`;
  stageContext.beginPath();
  stageContext.moveTo(centerX, centerY);
  stageContext.lineTo(endX, endY);
  stageContext.stroke();

  stageContext.fillStyle = `hsla(${(hue + 150) % 360}, 100%, 72%, ${0.18 + flash * 0.3})`;
  stageContext.beginPath();
  stageContext.arc(centerX, centerY, Math.max(12, radius * 0.22), 0, TAU);
  stageContext.fill();

  for (let i = 0; i < 6; i += 1) {
    const phase = angle + (TAU / 6) * i + time * 0.2;
    stageContext.fillStyle = `hsla(${(hue + i * 24) % 360}, 100%, 74%, ${0.16 + i * 0.04})`;
    stageContext.beginPath();
    stageContext.arc(
      centerX + Math.cos(phase) * (radius + 26 + flash * 8),
      centerY + Math.sin(phase) * (radius + 26 + flash * 8),
      4 + energy * 26,
      0,
      TAU,
    );
    stageContext.fill();
  }

  stageContext.restore();
}

function syncTransportReadout() {
  const clock = state.nodes.find((node) => node.type === "clock");
  dom.bpmReadout.textContent = clock ? clock.params.bpm.toFixed(0) : "0";
  dom.beatReadout.textContent = state.transport.beat.toFixed(2);
  dom.cpuReadout.textContent = state.transport.running ? "live" : "idle";
}

function createAudioEngine() {
  const AudioContextCtor = window.AudioContext || window.webkitAudioContext;
  const context = new AudioContextCtor();
  const scriptNode = context.createScriptProcessor(512, 0, 2);
  const gainNode = context.createGain();
  gainNode.gain.value = 0;
  scriptNode.connect(gainNode);
  gainNode.connect(context.destination);

  scriptNode.onaudioprocess = (event) => {
    const left = event.outputBuffer.getChannelData(0);
    const right = event.outputBuffer.getChannelData(1);

    for (let index = 0; index < left.length; index += 1) {
      const t = context.currentTime + index / context.sampleRate;
      const output = evaluateGraph(t, "audio", 1 / context.sampleRate).output;
      const sample = clamp(output?.audio ?? 0, -1, 1);
      left[index] = sample;
      right[index] = sample;
    }
  };

  return {
    async start() {
      if (context.state === "suspended") {
        await context.resume();
      }
      gainNode.gain.setTargetAtTime(0.82, context.currentTime, 0.02);
    },
    stop() {
      gainNode.gain.setTargetAtTime(0, context.currentTime, 0.03);
    },
  };
}

function formatParam(value) {
  return Math.abs(value) >= 100 ? value.toFixed(0) : value.toFixed(2);
}

function getNodeState(nodeId, mode) {
  const key = `${mode}:${nodeId}`;
  if (!state.runtime.nodeState[key]) {
    state.runtime.nodeState[key] = {
      value: 0,
      stage1: 0,
      stage2: 0,
    };
  }
  return state.runtime.nodeState[key];
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
