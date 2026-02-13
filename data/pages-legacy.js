// OpenHaldex-S3 ui-dev page controllers (ported from data/scripts.js)
// This file keeps map/canview/diag/ota logic local to ui-dev.

// Map editor controller:
// - bin editors + lock table
// - active-cell tracer from live telemetry
// - map list/load/save/import/export operations
function initMapPage() {
  const defaultSpeed = [0, 5, 10, 20, 40, 60, 80, 100, 140];
  const defaultThrottle = [0, 5, 10, 20, 40, 60, 80];
  const MAP_COLS = defaultSpeed.length;
  const MAP_ROWS = defaultThrottle.length;
  const defaultLock = [
    [0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 5, 5, 5, 5, 0, 0, 0],
    [0, 5, 10, 15, 15, 10, 5, 0, 0],
    [5, 10, 20, 25, 25, 20, 15, 10, 5],
    [10, 20, 30, 40, 40, 30, 25, 20, 15],
    [20, 30, 45, 60, 60, 50, 40, 30, 20],
  ];

  const state = {
    speed: [...defaultSpeed],
    throttle: [...defaultThrottle],
    lock: defaultLock.map((r) => [...r]),
  };

  let cellInputs = [];
  let activeCell = null;
  const statusEl = document.getElementById("status");
  const speedBinsEl = document.getElementById("speedBins");
  const throttleBinsEl = document.getElementById("throttleBins");
  const tableEl = document.getElementById("mapTable");
  const mapSelect = document.getElementById("mapSelect");
  const shapeColLabel = document.getElementById("shapeColLabel");
  const shapeStart = document.getElementById("shapeStart");
  const shapeEnd = document.getElementById("shapeEnd");
  const shapeStartVal = document.getElementById("shapeStartVal");
  const shapeEndVal = document.getElementById("shapeEndVal");
  const btnApplyShape = document.getElementById("btnApplyShape");
  const mapDisengageInput = document.getElementById("mapDisengageSpeed");
  const btnSaveMapDisengage = document.getElementById("btnSaveMapDisengage");
  const mapThrottleGateInput = document.getElementById("mapDisableThrottle");
  const mapSpeedGateInput = document.getElementById("mapDisableSpeed");
  const mapBroadcastToggle = document.getElementById("mapBroadcastBridge");
  const mapControllerToggle = document.getElementById("mapControllerEnabled");

  let headerCells = [];
  let selectedCol = null;

  function setStatus(msg) {
    if (statusEl) {
      statusEl.textContent = msg || "";
    }
  }

  function toInt(value, fallback = 0) {
    const n = parseInt(value, 10);
    return Number.isFinite(n) ? n : fallback;
  }

  function clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
  }

  function getMapDisengageValue() {
    if (!mapDisengageInput) return 0;
    const value = clamp(toInt(mapDisengageInput.value, 0), 0, 300);
    mapDisengageInput.value = String(value);
    return value;
  }

  function setMapDisengageValue(value) {
    if (!mapDisengageInput) return;
    mapDisengageInput.value = String(clamp(toInt(value, 0), 0, 300));
  }

  function getMapThrottleGateValue() {
    if (!mapThrottleGateInput) return 0;
    const value = clamp(toInt(mapThrottleGateInput.value, 0), 0, 100);
    mapThrottleGateInput.value = String(value);
    return value;
  }

  function setMapThrottleGateValue(value) {
    if (!mapThrottleGateInput) return;
    mapThrottleGateInput.value = String(clamp(toInt(value, 0), 0, 100));
  }

  function getMapSpeedGateValue() {
    if (!mapSpeedGateInput) return 0;
    const value = clamp(toInt(mapSpeedGateInput.value, 0), 0, 300);
    mapSpeedGateInput.value = String(value);
    return value;
  }

  function setMapSpeedGateValue(value) {
    if (!mapSpeedGateInput) return;
    mapSpeedGateInput.value = String(clamp(toInt(value, 0), 0, 300));
  }

  function summarizeMapModeSettings(settings) {
    const disengage = settings.disengage > 0 ? `Disengage below ${settings.disengage} km/h.` : "Disengage disabled.";
    const throttleGate = settings.disableThrottle > 0 ? `Throttle >= ${settings.disableThrottle}%.` : "Throttle gate off.";
    const speedGate = settings.disableSpeed > 0 ? `Disable above ${settings.disableSpeed} km/h.` : "High-speed gate off.";
    const broadcast = settings.broadcastOpenHaldexOverCAN ? "Bridge on." : "Bridge off.";
    const controller = settings.disableController ? "Controller off." : "Controller on.";
    return `${disengage} ${throttleGate} ${speedGate} ${broadcast} ${controller}`;
  }

  function applyMapModeSettingsFromStatus(data) {
    const disengage = data && data.disengageUnderSpeed ? data.disengageUnderSpeed.map : 0;
    setMapDisengageValue(disengage || 0);
    setMapThrottleGateValue(data ? data.disableThrottle : 0);
    setMapSpeedGateValue(data ? data.disableSpeed : 0);
    if (mapBroadcastToggle) mapBroadcastToggle.checked = !data || data.broadcastOpenHaldexOverCAN !== false;
    if (mapControllerToggle) mapControllerToggle.checked = !data || !Boolean(data.disableController);
    return {
      disengage: getMapDisengageValue(),
      disableThrottle: getMapThrottleGateValue(),
      disableSpeed: getMapSpeedGateValue(),
      broadcastOpenHaldexOverCAN: mapBroadcastToggle ? Boolean(mapBroadcastToggle.checked) : true,
      disableController: mapControllerToggle ? !Boolean(mapControllerToggle.checked) : false,
    };
  }

  function readMapModeSettingsPayload() {
    return {
      disableThrottle: getMapThrottleGateValue(),
      disableSpeed: getMapSpeedGateValue(),
      broadcastOpenHaldexOverCAN: mapBroadcastToggle ? Boolean(mapBroadcastToggle.checked) : true,
      disableController: mapControllerToggle ? !Boolean(mapControllerToggle.checked) : false,
      disengageUnderSpeed: {
        map: getMapDisengageValue(),
      },
    };
  }

  function normalizeBins(values, fallback, min, max, length) {
    const targetLength = Number.isInteger(length) && length > 0 ? length : fallback.length;
    const src = Array.isArray(values) ? values : [];
    const out = [];
    for (let i = 0; i < targetLength; i++) {
      const fallbackValue = i < fallback.length ? fallback[i] : fallback[fallback.length - 1] || 0;
      const raw = i < src.length ? src[i] : fallbackValue;
      out.push(clamp(toInt(raw, fallbackValue), min, max));
    }
    return out;
  }

  function normalizeLockTable(lockTable, rows, cols) {
    const normalized = [];
    for (let r = 0; r < rows; r++) {
      const srcRow = Array.isArray(lockTable) ? lockTable[r] : null;
      const row = [];
      for (let c = 0; c < cols; c++) {
        const raw = Array.isArray(srcRow) ? srcRow[c] : 0;
        row.push(clamp(toInt(raw, 0), 0, 100));
      }
      normalized.push(row);
    }
    return normalized;
  }

  function updateShapeLabels() {
    if (shapeStartVal && shapeStart) shapeStartVal.textContent = shapeStart.value;
    if (shapeEndVal && shapeEnd) shapeEndVal.textContent = shapeEnd.value;
  }

  function updateCellValue(r, c, value) {
    const cell = (cellInputs[r] || [])[c];
    if (!cell) return;
    const input = cell.querySelector("input");
    if (!input) return;
    input.value = value;
    applyCellColor(cell, value);
  }

  function setSelectedColumn(index) {
    selectedCol = Number.isInteger(index) ? index : null;
    headerCells.forEach((cell, i) => {
      cell.classList.toggle("active", selectedCol === i);
    });
    if (shapeColLabel) {
      shapeColLabel.textContent = selectedCol === null ? "None" : "S" + state.speed[selectedCol];
    }
  }

  function applyCellColor(cell, value) {
    if (!cell) return;
    const v = Math.max(0, Math.min(100, parseInt(value || 0, 10)));
    const isLight = document.body && document.body.dataset && document.body.dataset.theme === "light";
    const styles = window.getComputedStyle(document.body);
    const brandRgb = (styles.getPropertyValue("--brand-rgb") || "").trim();
    const textColor = (styles.getPropertyValue("--text") || "").trim();
    const alpha = 0.06 + (v / 100) * 0.26;
    const input = cell.querySelector("input");
    const fallbackRgb = isLight ? "0, 81, 206" : "187, 10, 48";
    cell.style.backgroundColor = `rgba(${brandRgb || fallbackRgb}, ${alpha.toFixed(3)})`;
    cell.style.color = textColor || (isLight ? "#0b1324" : "#e8e9ee");
    if (input) input.style.color = "inherit";
  }

  function repaintHeatmap() {
    for (let r = 0; r < state.throttle.length; r++) {
      for (let c = 0; c < state.speed.length; c++) {
        const cell = (cellInputs[r] || [])[c];
        if (!cell) continue;
        applyCellColor(cell, state.lock[r][c]);
      }
    }
  }

  function watchThemeChanges() {
    if (!document.body || typeof MutationObserver === "undefined") return;
    const observer = new MutationObserver((mutations) => {
      for (const mutation of mutations) {
        if (mutation.type === "attributes") {
          repaintHeatmap();
          break;
        }
      }
    });
    observer.observe(document.body, {
      attributes: true,
      attributeFilter: ["data-theme", "class"],
    });
  }

  function buildBins() {
    speedBinsEl.innerHTML = "";
    throttleBinsEl.innerHTML = "";
    state.speed.forEach((v, i) => {
      const input = document.createElement("input");
      input.type = "number";
      input.value = v;
      input.min = 0;
      input.max = 255;
      input.onchange = () => {
        const next = clamp(toInt(input.value, state.speed[i]), 0, 255);
        input.value = String(next);
        state.speed[i] = next;
      };
      speedBinsEl.appendChild(input);
    });
    state.throttle.forEach((v, i) => {
      const input = document.createElement("input");
      input.type = "number";
      input.value = v;
      input.min = 0;
      input.max = 100;
      input.onchange = () => {
        const next = clamp(toInt(input.value, state.throttle[i]), 0, 100);
        input.value = String(next);
        state.throttle[i] = next;
      };
      throttleBinsEl.appendChild(input);
    });
  }

  function buildTable() {
    tableEl.innerHTML = "";
    const thead = document.createElement("thead");
    const hrow = document.createElement("tr");
    hrow.appendChild(th("T/S"));
    headerCells = [];
    state.speed.forEach((s, c) => {
      const header = th("S" + s);
      header.classList.add("map-col-header");
      header.addEventListener("click", () => setSelectedColumn(c));
      headerCells.push(header);
      hrow.appendChild(header);
    });
    thead.appendChild(hrow);
    tableEl.appendChild(thead);

    const tbody = document.createElement("tbody");
    cellInputs = [];
    state.throttle.forEach((t, r) => {
      const row = document.createElement("tr");
      row.appendChild(th("T" + t));
      state.speed.forEach((_, c) => {
        const cell = document.createElement("td");
        const input = document.createElement("input");
        input.type = "text";
        input.inputMode = "numeric";
        input.setAttribute("pattern", "[0-9]*");
        input.setAttribute("aria-label", `Lock value T${t} S${state.speed[c]}`);
        input.value = state.lock[r][c];
        applyCellColor(cell, input.value);
        input.onchange = () => {
          const v = clamp(toInt(input.value, state.lock[r][c]), 0, 100);
          input.value = String(v);
          state.lock[r][c] = v;
          applyCellColor(cell, v);
        };
        cell.appendChild(input);
        row.appendChild(cell);
        if (!cellInputs[r]) cellInputs[r] = [];
        cellInputs[r][c] = cell;
      });
      tbody.appendChild(row);
    });
    tableEl.appendChild(tbody);
    if (selectedCol !== null && selectedCol < headerCells.length) {
      setSelectedColumn(selectedCol);
    } else {
      setSelectedColumn(null);
    }
  }

  function applyColumnShape(options = {}) {
    if (selectedCol === null) {
      if (!options.silent) setStatus("Select a column header to shape");
      return;
    }
    const start = parseInt(shapeStart ? shapeStart.value : "0", 10);
    const end = parseInt(shapeEnd ? shapeEnd.value : "0", 10);
    const rows = state.throttle.length;
    for (let r = 0; r < rows; r++) {
      const t = rows > 1 ? r / (rows - 1) : 0;
      const raw = start + (end - start) * t;
      const value = Math.max(0, Math.min(100, Math.round(raw)));
      state.lock[r][selectedCol] = value;
      updateCellValue(r, selectedCol, value);
    }
    if (!options.silent) {
      setStatus(`Shaped column S${state.speed[selectedCol]} (${start} -> ${end})`);
    }
  }

  function th(text) {
    const el = document.createElement("th");
    el.textContent = text;
    return el;
  }

  function binIndex(value, bins) {
    let idx = 0;
    for (let i = 0; i < bins.length; i++) {
      if (value >= bins[i]) idx = i;
    }
    return idx;
  }

  function setActiveCell(r, c) {
    const cell = (cellInputs[r] || [])[c];
    if (!cell) return;
    if (activeCell && activeCell !== cell) activeCell.classList.remove("map-active");
    activeCell = cell;
    activeCell.classList.add("map-active");
  }

  async function refreshTrace() {
    try {
      const data = await fetchJson("/api/status");
      const telem = data.telemetry || {};
      const speed = Number(telem.speed || 0);
      const throttle = Number(telem.throttle || 0);
      const r = binIndex(throttle, state.throttle);
      const c = binIndex(speed, state.speed);
      setActiveCell(r, c);
    } catch (e) {
      // ignore trace errors
    }
  }

  async function loadMapDisengageSetting(options = {}) {
    try {
      const data = await fetchJson("/api/status");
      const settings = applyMapModeSettingsFromStatus(data);
      if (!options.silent) {
        setStatus(summarizeMapModeSettings(settings));
      }
      return settings;
    } catch (e) {
      if (!options.silent) {
        setStatus("Mode settings load failed: " + e.message);
      }
      return null;
    }
  }

  async function saveMapDisengageSetting(options = {}) {
    const payload = readMapModeSettingsPayload();
    await fetchJson("/api/settings", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
    let settings = {
      disengage: payload.disengageUnderSpeed.map,
      disableThrottle: payload.disableThrottle,
      disableSpeed: payload.disableSpeed,
      broadcastOpenHaldexOverCAN: payload.broadcastOpenHaldexOverCAN,
      disableController: payload.disableController,
    };
    try {
      const loaded = await loadMapDisengageSetting({ silent: true });
      if (loaded) settings = loaded;
    } catch {
      // Preserve optimistic values when status reload is unavailable.
    }
    if (!options.silent) {
      setStatus("Saved mode settings. " + summarizeMapModeSettings(settings));
    }
    return settings;
  }

  async function refreshMapList(selectPath) {
    try {
      const data = await fetchJson("/api/maps");
      const current = data.current || "";
      mapSelect.innerHTML = "";
      (data.maps || []).forEach((entry) => {
        const opt = document.createElement("option");
        opt.value = entry.path;
        opt.textContent = `${entry.name} (${entry.path})`;
        mapSelect.appendChild(opt);
      });
      if (current && !Array.from(mapSelect.options).some((o) => o.value === current)) {
        const opt = document.createElement("option");
        const name = nameFromPath(current) || "current";
        opt.value = current;
        opt.textContent = `${name} (${current})`;
        mapSelect.appendChild(opt);
      }
      const target = selectPath || current;
      if (target) mapSelect.value = target;
    } catch (e) {
      setStatus("Map list failed: " + e.message);
    }
  }

  async function loadSelectedMap() {
    const path = mapSelect.value;
    if (!path) return;
    setStatus("Loading map...");
    try {
      await fetchJson("/api/maps/load", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ path }),
      });
      setStatus("Loaded: " + path);
      await loadFromDevice();
      refreshMapList(path);
    } catch (e) {
      setStatus("Load failed: " + e.message);
    }
  }

  async function loadFromDevice() {
    try {
      const data = await fetchJson("/api/map");
      state.speed = normalizeBins(data.speedBins, defaultSpeed, 0, 255, MAP_COLS);
      state.throttle = normalizeBins(data.throttleBins, defaultThrottle, 0, 100, MAP_ROWS);
      state.lock = normalizeLockTable(data.lockTable, MAP_ROWS, MAP_COLS);
      buildBins();
      buildTable();
      setStatus("Loaded current map");
    } catch (e) {
      // Keep editor usable even when device endpoints are unavailable.
      buildBins();
      buildTable();
      setStatus("Load failed: " + e.message);
    }
  }

  async function saveCurrent() {
    setStatus("Saving current map...");
    try {
      await fetchJson("/api/map", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          speedBins: state.speed,
          throttleBins: state.throttle,
          lockTable: state.lock,
        }),
      });
      setStatus("Saved current map");
    } catch (e) {
      setStatus("Save failed: " + e.message);
    }
  }

  async function saveMapAs() {
    const name = document.getElementById("mapName").value.trim();
    if (!name) {
      setStatus("Enter a map name");
      return;
    }
    try {
      await fetchJson("/api/map", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          speedBins: state.speed,
          throttleBins: state.throttle,
          lockTable: state.lock,
        }),
      });
      const res = await fetchJson("/api/maps/save", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name }),
      });
      setStatus("Saved: " + (res.path || name));
      refreshMapList(res.path || "");
    } catch (e) {
      setStatus("Save failed: " + e.message);
    }
  }

  async function deleteSelectedMap() {
    const path = mapSelect.value;
    if (!path) return;
    if (!confirm("Delete map: " + path + "?")) return;
    try {
      await fetchJson("/api/maps/delete", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ path }),
      });
      setStatus("Deleted: " + path);
      refreshMapList("");
    } catch (e) {
      setStatus("Delete failed: " + e.message);
    }
  }

  function exportTxt() {
    const header = ["T", "Throttle", ...state.speed.map((s) => "S" + s)].join("\t");
    const rows = state.throttle.map((t, i) => ["T" + t, t, ...state.lock[i]].join("\t"));
    const text = [header, ...rows].join("\n");
    const blob = new Blob([text], { type: "text/plain" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = "openhaldex-map.txt";
    document.body.appendChild(a);
    a.click();
    a.remove();
    URL.revokeObjectURL(url);
  }

  function fromTxt(text) {
    const lines = text.split(/\r?\n/).filter((l) => l.trim().length > 0);
    if (lines.length < 2) throw new Error("Invalid map file");
    const header = lines[0].split("\t");
    if (header.length < 3) throw new Error("Invalid map header");
    const parsedSpeed = header.slice(2).map((v) => parseInt(v.replace(/[ST]/g, ""), 10));
    const throttles = [];
    const lock = [];
    lines.slice(1).forEach((line) => {
      const parts = line.split("\t");
      const t = parseInt(parts[0].replace(/[ST]/g, ""), 10) || parseInt(parts[1], 10);
      throttles.push(t);
      lock.push(parts.slice(2).map((v) => parseInt(v, 10)));
    });
    state.speed = normalizeBins(parsedSpeed, defaultSpeed, 0, 255, MAP_COLS);
    state.throttle = normalizeBins(throttles, defaultThrottle, 0, 100, MAP_ROWS);
    state.lock = normalizeLockTable(lock, MAP_ROWS, MAP_COLS);
    buildBins();
    buildTable();
    setStatus("Loaded TXT map");
  }

  const btnRefresh = document.getElementById("btnRefresh");
  if (btnRefresh) btnRefresh.onclick = () => refreshMapList(mapSelect.value);

  const btnLoadMap = document.getElementById("btnLoadMap");
  if (btnLoadMap) btnLoadMap.onclick = loadSelectedMap;

  const btnLoad = document.getElementById("btnLoad");
  if (btnLoad) btnLoad.onclick = loadFromDevice;

  const btnSave = document.getElementById("btnSave");
  if (btnSave) btnSave.onclick = saveCurrent;

  const btnSaveMap = document.getElementById("btnSaveMap");
  if (btnSaveMap) btnSaveMap.onclick = saveMapAs;

  const btnDeleteMap = document.getElementById("btnDeleteMap");
  if (btnDeleteMap) btnDeleteMap.onclick = deleteSelectedMap;

  const btnDownload = document.getElementById("btnDownload");
  if (btnDownload) btnDownload.onclick = exportTxt;
  if (mapDisengageInput) {
    mapDisengageInput.onchange = () => {
      setMapDisengageValue(mapDisengageInput.value);
    };
  }
  if (mapThrottleGateInput) {
    mapThrottleGateInput.onchange = () => {
      setMapThrottleGateValue(mapThrottleGateInput.value);
    };
  }
  if (mapSpeedGateInput) {
    mapSpeedGateInput.onchange = () => {
      setMapSpeedGateValue(mapSpeedGateInput.value);
    };
  }
  if (btnSaveMapDisengage) {
    btnSaveMapDisengage.onclick = async () => {
      try {
        await saveMapDisengageSetting();
      } catch (e) {
        setStatus("Mode settings save failed: " + e.message);
      }
    };
  }
  if (shapeStart) {
    shapeStart.addEventListener("input", () => {
      updateShapeLabels();
      applyColumnShape({ silent: true });
    });
  }
  if (shapeEnd) {
    shapeEnd.addEventListener("input", () => {
      updateShapeLabels();
      applyColumnShape({ silent: true });
    });
  }
  if (btnApplyShape) btnApplyShape.onclick = () => applyColumnShape({ silent: false });
  const fileInput = document.getElementById("fileInput");
  if (fileInput) {
    fileInput.onchange = async (e) => {
      const file = e.target.files[0];
      if (!file) return;
      try {
        const txt = await file.text();
        fromTxt(txt);
        await saveCurrent();
        await refreshMapList("");
        setStatus("Loaded TXT map and saved current map");
      } catch (err) {
        setStatus(err.message || "TXT load failed");
      }
    };
  }

  watchThemeChanges();

  // Render an editable fallback matrix immediately, then hydrate from device.
  buildBins();
  buildTable();
  refreshMapList("");
  loadFromDevice();
  loadMapDisengageSetting({ silent: true });
  setInterval(refreshTrace, 250);
  updateShapeLabels();
}

// CAN view controller:
// - decoded/raw polling with bus + token filters
// - optional safe capture mode toggle
// - one-shot 30s text dump download
function initCanviewPage() {
  const nameAliases = {
    Motordrehzahl: "Engine speed",
    Lastsignal: "Engine load",
    Fahrpedalwert_oder_Drosselklapp: "Throttle position",
    Ladedruck: "Boost pressure",
    Kuehlmitteltemp__4_1__Kombi_2_: "Coolant temp",
    Oeltemperatur_4_1: "Oil temp",
    Aussentemperatur_gefiltert: "Outside temp (filtered)",
    Aussentemp__ungefiltert_4_1__Ko: "Outside temp (raw)",
    Kupplungssteifigkeit_Hinten__Is: "Rear clutch stiffness",
    Kupplungssteifigkeit_Mitte__Ist: "Center clutch stiffness",
    Kupplung_komplett_offen: "Clutch fully open",
    Geschwindigkeitsbegrenzung: "Speed limit active",
    Allrad_Warnlampe: "AWD warning lamp",
    Notlauf: "Limp mode",
    Ubertemperaturschutz__Allrad_1_: "AWD overtemp protection",
    Fehler_Allrad_Kupplung: "AWD clutch fault",
    Fehlerstatus_Kupplungssteifigke: "Clutch stiffness fault",
    Fehlerspeichereintrag_Allrad_1: "AWD fault stored",
  };

  function formatName(name) {
    if (!name) return "";
    const alias = nameAliases[name];
    return alias || name.replace(/_/g, " ");
  }

  let timer = null;
  let captureActive = false;
  let lastData = { decoded: [], raw: [] };

  const statusEl = document.getElementById("status");
  const captureStatusEl = document.getElementById("captureStatus");
  const decodedBody = document.querySelector("#decodedTable tbody");
  const rawBody = document.querySelector("#rawTable tbody");
  const busFilterEl = document.getElementById("busFilter");
  const filterEl = document.getElementById("filter");
  const presetEl = document.getElementById("diagPreset");
  const captureBtn = document.getElementById("btnCapture");

  function setStatus(msg) {
    statusEl.textContent = msg || "";
  }

  function setCaptureStatus(msg) {
    if (captureStatusEl) captureStatusEl.textContent = msg || "";
  }

  function updateCaptureUi() {
    if (captureBtn) {
      captureBtn.textContent = captureActive ? "Diagnostic Capture: On" : "Diagnostic Capture: Off";
    }
    setCaptureStatus(
      captureActive ? "Capture mode active: Controller OFF + Broadcast ON (settings locked)" : ""
    );
  }

  function busMatches(item, busFilter) {
    if (!busFilter || busFilter === "all") return true;
    const bus = String(item.bus || "").toLowerCase();
    if (!bus) return false;
    if (bus === busFilter) return true;
    if (busFilter === "chassis" && (bus.includes("chassis") || bus.includes("chs"))) return true;
    if (busFilter === "haldex" && (bus.includes("haldex") || bus.includes("hdx"))) return true;
    return false;
  }

  function normalizeToken(token) {
    const t = String(token || "")
      .trim()
      .toLowerCase();
    if (!t) return "";
    return t.startsWith("0x") ? t.slice(2) : t;
  }

  function splitTokens(text) {
    const input = String(text || "").trim();
    if (!input) return [];
    return input
      .split(/\s+/)
      .map(normalizeToken)
      .filter((t) => t.length > 0);
  }

  function mergeTokens(existing, extra) {
    const seen = new Set();
    const out = [];
    [...splitTokens(existing), ...splitTokens(extra)].forEach((t) => {
      if (!seen.has(t)) {
        seen.add(t);
        out.push(t);
      }
    });
    return out.join(" ");
  }

  function addSelectedPreset() {
    if (!filterEl || !presetEl || !presetEl.value) return;
    filterEl.value = mergeTokens(filterEl.value, presetEl.value);
    presetEl.value = "";
    redraw();
  }

  function decodedMatches(item, tokens) {
    if (!tokens || !tokens.length) return true;
    const base = String(item.name || "");
    const pretty = formatName(base);
    const idNum = Number(item.id);
    const idDec = String(item.id || "").toLowerCase();
    const idHex = Number.isFinite(idNum) ? idNum.toString(16).toLowerCase() : "";
    const fields = [
      idDec,
      idHex,
      base.toLowerCase(),
      pretty.toLowerCase(),
      String(item.value || "").toLowerCase(),
    ];
    return tokens.some((t) => fields.some((field) => field.includes(t)));
  }

  function rawMatches(item, tokens) {
    if (!tokens || !tokens.length) return true;
    const idNum = Number(item.id);
    const idDec = String(item.id || "").toLowerCase();
    const idHex = Number.isFinite(idNum) ? idNum.toString(16).toLowerCase() : "";
    const fields = [idDec, idHex, String(item.data || "").toLowerCase()];
    return tokens.some((t) => fields.some((field) => field.includes(t)));
  }

  function redraw() {
    const filterTokens = splitTokens(filterEl ? filterEl.value : "");
    const busFilter = busFilterEl ? busFilterEl.value : "all";
    renderDecoded(decodedBody, lastData.decoded || [], filterTokens, busFilter);
    renderRaw(rawBody, lastData.raw || [], busFilter, filterTokens);
  }

  async function poll() {
    const decodedLimit = parseInt(document.getElementById("decodedLimit").value || "48", 10);
    const rawLimit = parseInt(document.getElementById("rawLimit").value || "20", 10);
    const busFilter = busFilterEl ? busFilterEl.value : "all";
    try {
      const data = await fetchJson(
        "/api/canview?decoded=" +
          decodedLimit +
          "&raw=" +
          rawLimit +
          "&bus=" +
          encodeURIComponent(busFilter)
      );
      lastData.decoded = data.decoded || [];
      lastData.raw = data.raw || [];
      redraw();
      setStatus("Updated " + new Date().toLocaleTimeString());
    } catch (e) {
      setStatus("Fetch failed: " + e.message);
      redraw();
    }
  }

  async function downloadDump() {
    const busFilter = busFilterEl ? busFilterEl.value : "all";
    try {
      setStatus("Building 30s dump...");
      const res = await fetch("/api/canview/dump?seconds=30&bus=" + encodeURIComponent(busFilter));
      if (!res.ok) {
        throw new Error("HTTP " + res.status);
      }
      const blob = await res.blob();
      const url = URL.createObjectURL(blob);
      const a = document.createElement("a");
      const stamp = new Date().toISOString().replace(/[.:]/g, "-");
      a.href = url;
      a.download = `openhaldex-can-dump-${busFilter}-${stamp}.txt`;
      document.body.appendChild(a);
      a.click();
      a.remove();
      URL.revokeObjectURL(url);
      setStatus("Dump downloaded");
    } catch (e) {
      setStatus("Dump failed: " + e.message);
    }
  }

  async function refreshCaptureState() {
    try {
      const data = await fetchJson("/api/canview/capture");
      captureActive = !!data.active;
      updateCaptureUi();
    } catch (e) {
      setCaptureStatus("Capture status failed: " + e.message);
    }
  }

  async function toggleCaptureMode() {
    try {
      setCaptureStatus("Switching capture mode...");
      const data = await fetchJson("/api/canview/capture", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ active: !captureActive }),
      });
      captureActive = !!data.active;
      updateCaptureUi();
      setStatus(captureActive ? "Capture mode enabled" : "Capture mode disabled");
      await poll();
    } catch (e) {
      setCaptureStatus("Capture switch failed: " + e.message);
    }
  }

  function renderDecoded(body, list, filterTokens, busFilter) {
    body.innerHTML = "";
    list
      .filter((item) => busMatches(item, busFilter) && decodedMatches(item, filterTokens))
      .forEach((item) => {
        const tr = document.createElement("tr");
        const baseName = item.name || "";
        const dir = String(item.dir || "").toLowerCase();
        if (dir === "rx" || dir === "tx") tr.classList.add("dir-" + dir);
        if (item.generated) tr.classList.add("generated");
        tr.innerHTML = `<td>${item.bus || ""}</td><td>${item.dir || ""}</td><td>${item.id}</td><td title="${baseName}">${formatName(baseName)}</td><td>${item.value}</td><td>${item.unit || ""}</td><td>${ageMs(item.ts)}</td>`;
        body.appendChild(tr);
      });
  }

  function renderRaw(body, list, busFilter, filterTokens) {
    body.innerHTML = "";
    list
      .filter((item) => busMatches(item, busFilter) && rawMatches(item, filterTokens))
      .forEach((item) => {
        const tr = document.createElement("tr");
        const dir = String(item.dir || "").toLowerCase();
        if (dir === "rx" || dir === "tx") tr.classList.add("dir-" + dir);
        if (item.generated) tr.classList.add("generated");
        tr.innerHTML = `<td>${item.bus || ""}</td><td>${item.dir || ""}</td><td>${item.id}</td><td>${item.dlc}</td><td>${item.data}</td><td>${ageMs(item.ts)}</td>`;
        body.appendChild(tr);
      });
  }

  function ageMs(ts) {
    if (!ts) return "";
    const now = Date.now();
    return Math.max(0, now - ts) + " ms";
  }

  document.getElementById("btnStart").onclick = () => {
    const interval = parseInt(document.getElementById("interval").value || "500", 10);
    if (timer) clearInterval(timer);
    timer = setInterval(poll, interval);
    poll();
  };

  if (busFilterEl) {
    busFilterEl.onchange = () => {
      redraw();
      poll();
    };
  }
  if (filterEl) filterEl.oninput = redraw;
  if (presetEl) presetEl.onchange = addSelectedPreset;

  document.getElementById("btnStop").onclick = () => {
    if (timer) clearInterval(timer);
    timer = null;
  };

  const dumpBtn = document.getElementById("btnDump30");
  if (dumpBtn) dumpBtn.onclick = downloadDump;
  if (captureBtn) captureBtn.onclick = toggleCaptureMode;

  refreshCaptureState();
}
// Diagnostics page:
// combines status, telemetry, frame diagnostics, and network state.
function initDiagPage() {
  const el = (id) => document.getElementById(id);
  const statusEl = el("status");

  function pill(value) {
    if (value === true || value === 1) return '<span class="pill good">Yes</span>';
    if (value === false || value === 0) return '<span class="pill bad">No</span>';
    return `<span class="muted">${value ?? "-"}</span>`;
  }

  function fmt(val, suffix = "") {
    if (val === null || val === undefined) return "-";
    if (typeof val === "number") return `${val}${suffix}`;
    return `${val}${suffix}`;
  }

  function linkOrDash(value) {
    const v = String(value || "").trim();
    if (!v) return "-";
    return `<a href="http://${v}">${v}</a>`;
  }
  function haldexStateLabel(value) {
    if (value === null || value === undefined) return "-";
    const v = Number(value) || 0;
    const labels = [];
    if (v & (1 << 0)) labels.push("Clutch 1 report");
    if (v & (1 << 1)) labels.push("Temp protection");
    if (v & (1 << 2)) labels.push("Clutch 2 report");
    if (v & (1 << 3)) labels.push("Coupling open");
    if (v & (1 << 6)) labels.push("Speed limit");
    if (!labels.length) labels.push("None");
    return `${v} (${labels.join(", ")})`;
  }

  function fmtMs(val) {
    if (val === null || val === undefined) return "-";
    return `${val} ms`;
  }

  function fmtFrame(frame) {
    if (!frame || !frame.ok) return "-";
    const kind = frame.generated ? "GEN" : "BRIDGE";
    const age = frame.ageMs === undefined ? "-" : `${frame.ageMs} ms`;
    return `${frame.data || ""} (${kind}, ${age})`;
  }

  function setText(id, value) {
    const node = el(id);
    if (node) node.textContent = value;
  }
  function fmtUptime(ms) {
    if (!ms && ms !== 0) return "-";
    const sec = Math.floor(ms / 1000);
    const h = Math.floor(sec / 3600);
    const m = Math.floor((sec % 3600) / 60);
    const s = sec % 60;
    return `${h}h ${m}m ${s}s`;
  }

  async function poll() {
    try {
      const data = await fetchJson("/api/status");

      el("mode").textContent = data.mode || "-";
      el("controllerEnabled").innerHTML = pill(!data.disableController);
      el("broadcast").innerHTML = pill(data.broadcastOpenHaldexOverCAN);
      el("haldexGen").textContent = data.haldexGeneration ?? "-";

      const tel = data.telemetry || {};
      el("spec").textContent = fmt(tel.spec);
      el("act").textContent = fmt(tel.act);
      el("haldexState").textContent = haldexStateLabel(tel.haldexState);
      el("haldexEngagement").textContent = fmt(tel.haldexEngagement);

      el("speed").textContent = fmt(tel.speed, " km/h");
      el("rpm").textContent = fmt(tel.rpm, " rpm");
      el("throttle").textContent = fmt(tel.throttle, " %");
      el("boost").textContent = fmt(tel.boost, " kPa");
      el("clutch1").innerHTML = pill(tel.clutch1Report);
      el("clutch2").innerHTML = pill(tel.clutch2Report);

      const can = data.can || {};
      el("canReady").innerHTML = pill(can.ready);
      el("canChassis").innerHTML = pill(can.chassis);
      el("canHaldex").innerHTML = pill(can.haldex);
      el("canFailure").innerHTML = can.busFailure
        ? '<span class="pill bad">Failure</span>'
        : '<span class="pill good">OK</span>';
      el("lastChassis").textContent = fmtMs(can.lastChassisMs);
      el("lastHaldex").textContent = fmtMs(can.lastHaldexMs);

      el("tempProtection").innerHTML = pill(tel.tempProtection);
      el("couplingOpen").innerHTML = pill(tel.couplingOpen);

      const frameDiag = data.frameDiag || {};
      setText("frameTarget", fmt(frameDiag.lockTarget));
      setText("frameM1", fmtFrame(frameDiag.motor1));
      setText("frameM3", fmtFrame(frameDiag.motor3));
      setText("frameB1", fmtFrame(frameDiag.brakes1));
      setText("frameB2", fmtFrame(frameDiag.brakes2));
      setText("frameB3", fmtFrame(frameDiag.brakes3));

      el("uptime").textContent = fmtUptime(data.uptimeMs);

      try {
        const net = await fetchJson("/api/network");
        const mode = net.staConnected ? "STA+AP" : net.ap ? "AP" : "STA";
        el("netMode").textContent = mode;
        el("netStaIp").innerHTML = linkOrDash(net.staIp);
        el("netApIp").innerHTML = linkOrDash(net.apIp);
        el("netHost").innerHTML = linkOrDash(net.hostname);
        el("netInternet").innerHTML = pill(!!net.staConnected && !!net.internet);
      } catch (__) {
        el("netMode").textContent = "-";
        el("netStaIp").textContent = "-";
        el("netApIp").textContent = "-";
        el("netHost").textContent = "-";
        el("netInternet").textContent = "-";
      }

      statusEl.textContent = "Last update: " + new Date().toLocaleTimeString();
    } catch (e) {
      statusEl.textContent = "Status fetch failed: " + e.message;
    }
  }

  poll();
  setInterval(poll, 1000);
}

// OTA page:
// - local file upload flow
// - hotspot credential management
// - online update check/install polling UI
function initOtaPage() {
  const form = document.getElementById("otaForm");
  const statusEl = document.getElementById("status");
  const wifiStatus = document.getElementById("wifiStatus");
  const updateStatus = document.getElementById("updateStatus");
  const wifiStaEnable = document.getElementById("wifiStaEnable");
  const wifiSsid = document.getElementById("wifiSsid");
  const wifiPass = document.getElementById("wifiPass");
  const wifiApPass = document.getElementById("wifiApPass");
  const wifiStaStatus = document.getElementById("wifiStaStatus");
  const wifiApStatus = document.getElementById("wifiApStatus");
  const btnWifiSaveSta = document.getElementById("btnWifiSaveSta");
  const btnWifiClearSta = document.getElementById("btnWifiClearSta");
  const btnWifiSaveAp = document.getElementById("btnWifiSaveAp");
  const btnWifiClearAp = document.getElementById("btnWifiClearAp");

  form.addEventListener("submit", async (e) => {
    e.preventDefault();
    const file = document.getElementById("bin").files[0];
    if (!file) return;
    statusEl.textContent = "Uploading...";

    const data = new FormData();
    data.append("update", file, file.name);

    try {
      const res = await fetch("/ota/update", { method: "POST", body: data });
      const text = await res.text();
      statusEl.textContent = res.ok ? "Update OK, rebooting..." : "Update failed: " + text;
    } catch (err) {
      statusEl.textContent = "Upload error: " + err;
    }
  });
  async function loadWifi() {
    if (!wifiSsid || !wifiStaEnable) return;
    try {
      const data = await fetchJson("/api/wifi");
      wifiSsid.value = data.ssid || "";
      if (wifiPass) wifiPass.value = "";
      if (wifiApPass) wifiApPass.value = "";
      wifiStaEnable.checked = !!data.staEnabled;
      const apMode = data.apPasswordSet ? "AP password set" : "AP open";
      if (wifiStaStatus) {
        wifiStaStatus.textContent = data.ssid
          ? `Saved hotspot: ${data.ssid} | STA ${data.staEnabled ? "enabled" : "disabled"}`
          : "No saved hotspot";
      }
      if (wifiApStatus) {
        wifiApStatus.textContent = data.apPasswordSet ? "AP password is set" : "AP is open (no password)";
      }
      if (wifiStatus) {
        wifiStatus.textContent = data.ssid ? `Saved SSID: ${data.ssid} | ${apMode}` : `No saved hotspot | ${apMode}`;
      }
    } catch (e) {
      if (wifiStaStatus) wifiStaStatus.textContent = "STA fetch failed: " + e.message;
      if (wifiApStatus) wifiApStatus.textContent = "AP fetch failed: " + e.message;
      if (wifiStatus) wifiStatus.textContent = "Wi-Fi fetch failed: " + e.message;
    }
  }

  async function saveStaSettings() {
    if (!wifiSsid || !wifiStaEnable) return;
    const payload = {
      ssid: wifiSsid.value || "",
      staEnabled: !!wifiStaEnable.checked,
    };
    if (wifiPass) {
      const staPass = String(wifiPass.value || "").trim();
      if (staPass.length > 0) {
        payload.password = staPass;
      }
    }
    try {
      await fetchJson("/api/wifi", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
      });
      if (wifiPass) wifiPass.value = "";
      await loadWifi();
      if (wifiStaStatus) wifiStaStatus.textContent = "STA settings saved";
      if (wifiStatus) wifiStatus.textContent = "STA settings saved";
    } catch (e) {
      if (wifiStaStatus) wifiStaStatus.textContent = "STA save failed: " + e.message;
      if (wifiStatus) wifiStatus.textContent = "STA save failed: " + e.message;
    }
  }

  async function clearStaSettings() {
    if (!wifiSsid || !wifiStaEnable) return;
    wifiSsid.value = "";
    if (wifiPass) wifiPass.value = "";
    wifiStaEnable.checked = false;
    try {
      await fetchJson("/api/wifi", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ ssid: "", password: "", staEnabled: false }),
      });
      await loadWifi();
      if (wifiStaStatus) wifiStaStatus.textContent = "STA settings cleared";
      if (wifiStatus) wifiStatus.textContent = "STA settings cleared";
    } catch (e) {
      if (wifiStaStatus) wifiStaStatus.textContent = "STA clear failed: " + e.message;
      if (wifiStatus) wifiStatus.textContent = "STA clear failed: " + e.message;
    }
  }

  async function saveApSettings() {
    if (!wifiApPass) return;
    const apPass = String(wifiApPass.value || "");
    if (!apPass.length) {
      if (wifiApStatus) wifiApStatus.textContent = "Enter AP password or use Clear AP Password";
      return;
    }
    try {
      await fetchJson("/api/wifi", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ apPassword: apPass }),
      });
      wifiApPass.value = "";
      await loadWifi();
      if (wifiApStatus) wifiApStatus.textContent = "AP password saved";
      if (wifiStatus) wifiStatus.textContent = "AP password saved";
    } catch (e) {
      if (wifiApStatus) wifiApStatus.textContent = "AP save failed: " + e.message;
      if (wifiStatus) wifiStatus.textContent = "AP save failed: " + e.message;
    }
  }

  async function clearApSettings() {
    if (!wifiApPass) return;
    wifiApPass.value = "";
    try {
      await fetchJson("/api/wifi", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ apPassword: "" }),
      });
      await loadWifi();
      if (wifiApStatus) wifiApStatus.textContent = "AP password cleared (AP is open)";
      if (wifiStatus) wifiStatus.textContent = "AP password cleared";
    } catch (e) {
      if (wifiApStatus) wifiApStatus.textContent = "AP clear failed: " + e.message;
      if (wifiStatus) wifiStatus.textContent = "AP clear failed: " + e.message;
    }
  }

  if (btnWifiSaveSta) btnWifiSaveSta.onclick = saveStaSettings;
  if (btnWifiClearSta) btnWifiClearSta.onclick = clearStaSettings;
  if (btnWifiSaveAp) btnWifiSaveAp.onclick = saveApSettings;
  if (btnWifiClearAp) btnWifiClearAp.onclick = clearApSettings;

  function formatBytes(bytes) {
    const b = Number(bytes) || 0;
    if (b >= 1024 * 1024) return (b / (1024 * 1024)).toFixed(2) + " MB";
    if (b >= 1024) return (b / 1024).toFixed(1) + " KB";
    return b + " B";
  }

  function formatSpeed(bps) {
    const b = Number(bps) || 0;
    if (b <= 0) return "";
    return formatBytes(b) + "/s";
  }

  function buildProgressText(data) {
    const stage = data.stage || "update";
    const done = Number(data.bytesDone) || 0;
    const total = Number(data.bytesTotal) || 0;
    const speed = Number(data.speedBps) || 0;
    let text = "Installing " + stage;
    if (total > 0) {
      const pct = Math.min(100, (done / total) * 100).toFixed(1);
      text += ": " + pct + "% (" + formatBytes(done) + " / " + formatBytes(total) + ")";
    } else if (done > 0) {
      text += ": " + formatBytes(done);
    }
    const speedText = formatSpeed(speed);
    if (speedText) text += " | " + speedText;
    return text;
  }
  async function installUpdate() {
    if (!updateStatus) return;
    updateStatus.textContent = "Starting update...";
    try {
      const res = await fetch("/api/update/install", { method: "POST" });
      if (!res.ok) {
        const text = await res.text();
        updateStatus.textContent = "Update start failed: " + text;
        return;
      }
      updateStatus.textContent = "Installing update...";
    } catch (e) {
      updateStatus.textContent = "Update start failed: " + e.message;
    }
  }

  async function refreshUpdate() {
    if (!updateStatus) return;
    try {
      const data = await fetchJson("/api/update");
      const current = data.current || "-";
      const latest = data.latest || "";

      if (data.installing) {
        updateStatus.textContent = buildProgressText(data) || "Installing update...";
        return;
      }

      if (data.installError) {
        updateStatus.textContent = "Update failed: " + data.installError;
        return;
      }

      if (data.available && latest) {
        updateStatus.innerHTML =
          'Update available: <button id="btnInstallUpdate">Install ' +
          latest +
          "</button> (current " +
          current +
          ")";
      } else if (latest) {
        updateStatus.textContent = "Up to date: " + current;
      } else if (data.error) {
        updateStatus.textContent = "Update: " + current + " (" + data.error + ")";
      } else {
        updateStatus.textContent = "Update: " + current;
      }

      const btn = document.getElementById("btnInstallUpdate");
      if (btn) btn.onclick = installUpdate;
    } catch (e) {
      updateStatus.textContent = "Update check failed: " + e.message;
    }
  }
  loadWifi();
  refreshUpdate();
  setInterval(refreshUpdate, 2000);
}
