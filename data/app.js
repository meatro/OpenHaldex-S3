// OpenHaldex-S3 UI
// (c) 2026 SpringfieldVW.com
// Licensed under the OpenHaldex-S3 UI Attribution License (see /data/LICENSE)

const menuToggle = document.querySelector(".menu-toggle");
const slideMenu = document.querySelector(".slide-menu");
const menuBackdrop = document.querySelector(".menu-backdrop");
const menuClose = document.querySelector(".menu-close");
const menuItems = document.querySelectorAll(".menu-items a");

const themeToggle = document.querySelector(".theme-toggle");
const fullscreenToggle = document.querySelector(".fullscreen-toggle");

const buttons = document.querySelectorAll(".btn-circle");
const ratioSlider = document.querySelector(".ratio-slider");
const ratioDisplay = document.querySelector(".ratio-display");
const frontNumber = document.querySelector('.ratio-number[data-position="front"]');
const rearNumber = document.querySelector('.ratio-number[data-position="rear"]');
const ratioLabels = document.querySelectorAll(".ratio-label");
const ratioSeparator = document.querySelector(".ratio-separator");
const ratioLine = document.querySelector(".ratio-line");
let lastLockRearBias = 50;

function setMenuState(isOpen) {
  if (!slideMenu || !menuBackdrop) {
    return;
  }

  slideMenu.classList.toggle("active", isOpen);
  slideMenu.setAttribute("aria-hidden", String(!isOpen));

  menuBackdrop.classList.toggle("active", isOpen);
  menuBackdrop.hidden = !isOpen;

  if (menuToggle) {
    menuToggle.setAttribute("aria-expanded", String(isOpen));
  }
}

function isEditableTarget(element) {
  if (!element) {
    return false;
  }

  if (
    element instanceof HTMLInputElement ||
    element instanceof HTMLTextAreaElement ||
    element instanceof HTMLSelectElement
  ) {
    return true;
  }

  return Boolean(element.isContentEditable);
}

function getStoredTheme() {
  try {
    const saved = localStorage.getItem("theme");
    if (saved === "light" || saved === "dark") {
      return saved;
    }
    const legacy = localStorage.getItem("ohTheme");
    return legacy === "light" || legacy === "dark" ? legacy : null;
  } catch {
    return null;
  }
}

function applyTheme(theme) {
  if (!document.body) {
    return;
  }
  const normalized = theme === "light" ? "light" : "dark";
  if (document.documentElement) {
    document.documentElement.dataset.theme = normalized;
    document.documentElement.classList.toggle("dark", normalized === "dark");
  }
  document.body.dataset.theme = normalized;
  document.body.classList.toggle("dark", normalized === "dark");
}

function storeTheme(theme) {
  try {
    const normalized = theme === "light" ? "light" : "dark";
    localStorage.setItem("theme", normalized);
    localStorage.setItem("ohTheme", normalized);
  } catch {
    // Ignore storage failures in restricted browser contexts.
  }
}

function updateThemeToggleLabel() {
  if (!themeToggle) {
    return;
  }

  const current = document.body.dataset.theme;
  const next = current === "dark" ? "light" : "dark";
  themeToggle.setAttribute("aria-label", `Switch to ${next} theme`);
}

function toggleTheme() {
  const current = document.body.dataset.theme;
  const next = current === "dark" ? "light" : "dark";
  applyTheme(next);
  storeTheme(next);
  updateThemeToggleLabel();
}

function updateFullscreenLabel() {
  if (!fullscreenToggle) {
    return;
  }

  const inFullscreen = Boolean(document.fullscreenElement);
  fullscreenToggle.setAttribute(
    "aria-label",
    inFullscreen ? "Exit fullscreen" : "Enter fullscreen"
  );
}

function getModeLabel(button) {
  const mode = button?.dataset?.mode;
  if (mode) {
    return mode.toUpperCase();
  }

  const text = button?.querySelector(".btn-icon")?.textContent || "";
  return text.trim().toUpperCase();
}

function updateRatioProgress(value, maxOverride) {
  if (!ratioLine || !ratioSlider) {
    return;
  }

  const max = Number.isFinite(Number(maxOverride))
    ? Number(maxOverride)
    : parseInt(ratioSlider.max, 10);
  const safeMax = max > 0 ? max : 100;
  const clamped = Math.max(0, Math.min(safeMax, Number(value) || 0));
  const percent = (clamped / safeMax) * 100;
  ratioLine.style.setProperty("--progress", `${percent}%`);
}

function setMode(button) {
  if (!button || !ratioSlider || !ratioDisplay || !frontNumber || !rearNumber) {
    return;
  }

  const isLockMode = button.dataset.mode === "lock";
  const isMeterMode =
    button.dataset.mode === "speed" ||
    button.dataset.mode === "throttle" ||
    button.dataset.mode === "map" ||
    button.dataset.mode === "rpm";

  buttons.forEach((candidate) => {
    const active = candidate === button;
    candidate.classList.toggle("active", active);
    candidate.setAttribute("aria-pressed", String(active));
  });

  if (isLockMode) {
    ratioSlider.classList.add("slider-enabled");
    ratioSlider.classList.remove("slider-meter");
    ratioSlider.disabled = false;
    ratioSlider.setAttribute("aria-disabled", "false");
    ratioSlider.max = "50";
    ratioSlider.step = "10";
    ratioDisplay.classList.remove("single-text");

    const rear = clampInt(lastLockRearBias, 0, 50);
    ratioSlider.value = String(rear);
    const front = 100 - rear;

    frontNumber.textContent = String(front);
    rearNumber.textContent = String(rear);
    ratioLabels.forEach((label) => {
      label.style.display = "block";
    });

    if (ratioSeparator) {
      ratioSeparator.style.display = "block";
    }

    updateRatioProgress(rear);
    return;
  }

  if (isMeterMode) {
    ratioSlider.classList.remove("slider-enabled");
    ratioSlider.classList.add("slider-meter");
    ratioSlider.disabled = true;
    ratioSlider.setAttribute("aria-disabled", "true");
    ratioSlider.max = "100";
    ratioSlider.step = "1";
    ratioDisplay.classList.add("single-text");
    frontNumber.textContent = getModeLabel(button);
    rearNumber.textContent = "";
    ratioLabels.forEach((label) => {
      label.style.display = "none";
    });
    if (ratioSeparator) {
      ratioSeparator.style.display = "none";
    }

    // Meter modes start from zero and advance only from live request telemetry.
    ratioSlider.value = "0";
    updateRatioProgress(0, 100);
    return;
  }

  ratioSlider.classList.remove("slider-enabled");
  ratioSlider.classList.remove("slider-meter");
  ratioSlider.disabled = true;
  ratioSlider.setAttribute("aria-disabled", "true");
  ratioDisplay.classList.add("single-text");

  frontNumber.textContent = getModeLabel(button);
  rearNumber.textContent = "";
  ratioLabels.forEach((label) => {
    label.style.display = "none";
  });

  if (ratioSeparator) {
    ratioSeparator.style.display = "none";
  }

  if (ratioLine) {
    ratioLine.style.setProperty("--progress", "100%");
  }
}

function lockModeFromRearBias(rearBias) {
  const rear = clampInt(rearBias, 0, 50);
  if (rear >= 50) {
    return "5050";
  }
  if (rear >= 40) {
    return "6040";
  }
  if (rear >= 30) {
    return "7030";
  }
  if (rear >= 20) {
    return "8020";
  }
  if (rear >= 10) {
    return "9010";
  }
  return "FWD";
}

function modeFromHomeButton(button) {
  const mode = String(button?.dataset?.mode || "").toLowerCase();
  if (mode === "throttle") {
    return "THROTTLE";
  }
  if (mode === "speed") {
    return "SPEED";
  }
  if (mode === "rpm") {
    return "RPM";
  }
  if (mode === "map") {
    return "MAP";
  }
  if (mode === "off") {
    return "STOCK";
  }
  if (mode === "lock" && ratioSlider) {
    return lockModeFromRearBias(lastLockRearBias);
  }
  return "";
}

async function pushHomeMode(button) {
  const mode = modeFromHomeButton(button);
  if (!mode) {
    return;
  }
  await apiJson("/api/mode", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ mode }),
  });
}

function clampInt(value, min, max) {
  const n = Number(value);
  if (!Number.isFinite(n)) {
    return min;
  }
  return Math.max(min, Math.min(max, Math.round(n)));
}

function initHomeTelemetry() {
  if (!ratioSlider || !frontNumber || !rearNumber || !buttons.length) {
    return;
  }

  let pollTimer = null;

  const buttonByMode = {};
  buttons.forEach((button) => {
    const mode = String(button.dataset.mode || "").toLowerCase();
    if (mode) {
      buttonByMode[mode] = button;
    }
  });

  const modeToButton = (status) => {
    if (status?.disableController && buttonByMode.off) {
      return buttonByMode.off;
    }
    const mode = String(status?.mode || "").toUpperCase();
    if (mode === "SPEED" && buttonByMode.speed) {
      return buttonByMode.speed;
    }
    if (mode === "THROTTLE" && buttonByMode.throttle) {
      return buttonByMode.throttle;
    }
    if (mode === "MAP" && buttonByMode.map) {
      return buttonByMode.map;
    }
    if ((mode === "STOCK" || mode === "PASSTHRU") && buttonByMode.off) {
      return buttonByMode.off;
    }
    if (mode === "RPM" && buttonByMode.rpm) {
      return buttonByMode.rpm;
    }
    if (
      mode === "5050" ||
      mode === "6040" ||
      mode === "7030" ||
      mode === "7525" ||
      mode === "8020" ||
      mode === "9010" ||
      mode === "FWD"
    ) {
      return buttonByMode.lock || null;
    }
    return null;
  };

  const modeToRear = (statusMode) => {
    const mode = String(statusMode || "").toUpperCase();
    if (mode === "5050") {
      return 50;
    }
    if (mode === "6040") {
      return 40;
    }
    if (mode === "7030" || mode === "7525") {
      return 30;
    }
    if (mode === "8020") {
      return 20;
    }
    if (mode === "9010") {
      return 10;
    }
    if (mode === "FWD") {
      return 0;
    }
    return null;
  };

  const updateRatioMeterFromStatus = (status) => {
    const mode = String(status?.mode || "").toUpperCase();
    const telemetry = status?.telemetry || {};

    const activeMode = document.querySelector(".btn-circle.active")?.dataset?.mode || "";
    const isMeterMode =
      activeMode === "speed" ||
      activeMode === "throttle" ||
      activeMode === "map" ||
      activeMode === "rpm";
    if (!isMeterMode) {
      return;
    }

    // Ignore stale status during mode transitions.
    if (
      (activeMode === "speed" && mode !== "SPEED") ||
      (activeMode === "throttle" && mode !== "THROTTLE") ||
      (activeMode === "map" && mode !== "MAP") ||
      (activeMode === "rpm" && mode !== "RPM")
    ) {
      ratioSlider.value = "0";
      updateRatioProgress(0, 100);
      return;
    }

    // Request meter normalization keeps compatibility with request scale (30..170)
    // while rendering a 0..100 rear bias meter.
    const rawSpec = Number(telemetry.spec);
    const specPercent = Number.isFinite(rawSpec) ? Math.max(0, Math.min(100, rawSpec)) : 0;
    const requestRaw = 30 + specPercent * 1.4;
    const rear =
      requestRaw <= 30
        ? 0
        : Math.max(0, Math.min(100, Math.round(((requestRaw - 30) / 140) * 100)));
    const front = 100 - rear;
    ratioSlider.value = String(rear);
    updateRatioProgress(rear, 100);

    const values = document.querySelectorAll(".data-item");
    values.forEach((node) => {
      const labelNode = node.querySelector(".data-label");
      const valueNode = node.querySelector(".data-value");
      if (!labelNode || !valueNode) {
        return;
      }
      const label = labelNode.textContent.trim().toLowerCase();
      if (label === "setting") {
        valueNode.textContent = `${front} / ${rear}`;
      } else if (label === "mode") {
        valueNode.textContent = mode;
      } else if (label === "speed" && Number.isFinite(Number(telemetry.speed))) {
        valueNode.textContent = `${Math.round(Number(telemetry.speed))} km/h`;
      } else if (label === "engine speed" && Number.isFinite(Number(telemetry.rpm))) {
        valueNode.textContent = `${Math.round(Number(telemetry.rpm))} rpm`;
      } else if (label === "boost" && Number.isFinite(Number(telemetry.boost))) {
        valueNode.textContent = `${Number(telemetry.boost).toFixed(1)} kPa`;
      } else if (label === "throttle" && Number.isFinite(Number(telemetry.throttle))) {
        valueNode.textContent = `${Math.round(Number(telemetry.throttle))}`;
      }
    });
  };

  const syncFromStatus = async () => {
    try {
      const status = await apiJson("/api/status");
      const target = modeToButton(status);
      if (target && target !== document.querySelector(".btn-circle.active")) {
        setMode(target);
      }

      if (target && target.dataset.mode === "lock") {
        const lockRear = modeToRear(status.mode);
        if (Number.isFinite(lockRear)) {
          lastLockRearBias = clampInt(lockRear, 0, 50);
          ratioSlider.value = String(lockRear);
          frontNumber.textContent = String(100 - lockRear);
          rearNumber.textContent = String(lockRear);
          updateRatioProgress(lockRear, 50);
        }
      }

      updateRatioMeterFromStatus(status);
    } catch {
      // Keep UI responsive even when status polling fails.
    }
  };

  syncFromStatus();
  pollTimer = window.setInterval(syncFromStatus, 1000);
  window.addEventListener("beforeunload", () => {
    if (pollTimer) {
      window.clearInterval(pollTimer);
    }
  });
}

async function apiJson(url, options) {
  const res = await fetch(url, options);
  if (!res.ok) {
    let detail = "";
    try {
      detail = await res.text();
    } catch {
      detail = "";
    }
    throw new Error(`HTTP ${res.status}${detail ? ` ${detail}` : ""}`);
  }
  return res.json();
}

// Compatibility alias used by legacy ui-dev page controllers.
async function fetchJson(url, options) {
  return apiJson(url, options);
}

async function apiText(url, options) {
  const res = await fetch(url, options);
  if (!res.ok) {
    let detail = "";
    try {
      detail = await res.text();
    } catch {
      detail = "";
    }
    throw new Error(`HTTP ${res.status}${detail ? ` ${detail}` : ""}`);
  }
  return res.text();
}

function formatBytes(bytes) {
  const n = Number(bytes || 0);
  if (n < 1024) {
    return `${n} B`;
  }
  if (n < 1024 * 1024) {
    return `${(n / 1024).toFixed(1)} KB`;
  }
  return `${(n / (1024 * 1024)).toFixed(2)} MB`;
}

function sanitizeFilename(name) {
  return String(name || "log").replace(/[\\/:*?"<>|]+/g, "_");
}

function initSetupPage() {
  const signalPicker = document.getElementById("signal-picker");
  const signalPickerToggle = document.getElementById("signal-picker-toggle");
  const signalPickerValue = document.getElementById("signal-picker-value");
  const signalPickerMenu = document.getElementById("signal-picker-menu");
  const mapSelectedButton = document.getElementById("map-selected-button");
  const addDashButton = document.getElementById("toggle-display-button");
  const mappedInputList = document.getElementById("mapped-input-list");
  const displaySignalList = document.getElementById("display-signal-list");
  const mappingStatus = document.getElementById("mapping-status");
  const haldexGenPicker = document.getElementById("haldex-gen-picker");
  const haldexGenToggle = document.getElementById("haldex-gen-toggle");
  const haldexGenValue = document.getElementById("haldex-gen-value");
  const haldexGenMenu = document.getElementById("haldex-gen-menu");
  const clearMappingsButton = document.getElementById("clear-mappings");
  const saveProfileButton = document.getElementById("save-profile");

  if (
    !signalPicker ||
    !signalPickerToggle ||
    !signalPickerValue ||
    !signalPickerMenu ||
    !mappedInputList ||
    !displaySignalList
  ) {
    return;
  }

  const LOCAL_PROFILE_KEY = "ohSetupProfile";
  const requiredInputs = [
    { key: "speed", label: "Speed" },
    { key: "throttle", label: "Throttle" },
    { key: "rpm", label: "Engine RPM" },
  ];
  const dashSlots = Array.from({ length: 8 }).map((_, idx) => ({
    key: `dash_${idx + 1}`,
    label: `Dashboard ${idx + 1}`,
  }));
  const defaultMappings = Object.fromEntries(requiredInputs.map((item) => [item.key, ""]));
  const defaultDashMappings = Object.fromEntries(dashSlots.map((slot) => [slot.key, ""]));

  let decodedSignals = [];
  let signalById = new Map();
  let selectedSignalId = "";
  let selectedInputKey = requiredInputs[0].key;
  let selectedDashKey = dashSlots[0].key;
  let mappings = { ...defaultMappings };
  let dashMappings = { ...defaultDashMappings };
  let currentHaldexGen = "2";
  let pollTimer = null;
  let pollBusy = false;
  let deferredSignalPickerRender = false;
  let deferredSetupRender = false;

  function escapeHtml(value) {
    return String(value || "")
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;")
      .replace(/'/g, "&#39;");
  }

  function normalizeSignalDisplayValue(value, unit = "", signalName = "") {
    const n = Number(value);
    if (!Number.isFinite(n)) {
      return value;
    }

    const normalizedUnit = String(unit || "")
      .trim()
      .toLowerCase();
    const normalizedName = String(signalName || "")
      .trim()
      .toLowerCase();

    // Some decoded RPM signals are published at 1/100 scale (8.25 -> 825).
    // Normalize here so setup selection matches the dashboard/telemetry readout.
    if (normalizedUnit === "rpm" || normalizedName.includes("rpm")) {
      const abs = Math.abs(n);
      if (abs > 0 && abs < 20) {
        const scaled = n * 100;
        if (Math.abs(scaled) >= 100 && Math.abs(scaled) <= 12000) {
          return scaled;
        }
      }
    }

    return n;
  }

  function formatValue(value, unit = "", signalName = "") {
    if (value === null || value === undefined || value === "") {
      return "--";
    }
    const n = Number(normalizeSignalDisplayValue(value, unit, signalName));
    if (!Number.isFinite(n)) {
      return String(value);
    }

    const normalizedUnit = String(unit || "")
      .trim()
      .toLowerCase();
    if (normalizedUnit === "rpm") {
      return Math.abs(n) >= 20 ? String(Math.round(n)) : n.toFixed(2).replace(/\.?0+$/, "");
    }

    if (Math.abs(n) >= 1000) {
      return String(Math.round(n));
    }
    if (Math.abs(n) >= 100) {
      return n.toFixed(1).replace(/\.?0+$/, "");
    }
    return n.toFixed(2).replace(/\.?0+$/, "");
  }

  function formatSignal(item) {
    const bus = String(item?.bus || "all").toLowerCase();
    const id = Number(item?.id);
    const frame = Number.isFinite(id)
      ? `0x${id.toString(16).toUpperCase()}`
      : String(item?.id || "");
    const signal = String(item?.name || "Signal")
      .replace(/_/g, " ")
      .trim();
    const unit = String(item?.unit || "").trim();
    const hz = Number(item?.hz || 0);
    const value = item?.value;
    const key = `${bus}|${frame}|${signal}|${unit}`.toLowerCase();
    return { key, bus, frame, signal, unit, hz, value };
  }

  function readProfile() {
    try {
      const raw = localStorage.getItem(LOCAL_PROFILE_KEY);
      if (!raw) {
        return null;
      }
      const parsed = JSON.parse(raw);
      return parsed && typeof parsed === "object" ? parsed : null;
    } catch {
      return null;
    }
  }

  function writeProfile() {
    const profile = {
      haldexGeneration: Number(currentHaldexGen) || 2,
      mappings,
      dashMappings,
      updatedAt: new Date().toISOString(),
    };
    try {
      localStorage.setItem(LOCAL_PROFILE_KEY, JSON.stringify(profile));
    } catch {
      // Ignore localStorage failures.
    }
  }

  function setStatus(message, isPending = false) {
    if (!mappingStatus) {
      return;
    }
    mappingStatus.textContent = message;
    mappingStatus.classList.toggle("pending", Boolean(isPending));
  }

  function getSignalById(signalId) {
    return signalById.get(String(signalId || "").toLowerCase()) || null;
  }

  function signalSummary(signalId) {
    const s = getSignalById(signalId);
    if (!s) {
      return { name: "Not Assigned", value: "--" };
    }
    const value = `${formatValue(s.value, s.unit, s.signal)}${s.unit ? ` ${s.unit}` : ""}`;
    return {
      name: `${s.frame} ${s.signal}`.trim(),
      value,
    };
  }

  function isSignalPickerOpen() {
    return signalPicker.classList.contains("open") && !signalPickerMenu.hidden;
  }

  function setPickerOpen(isOpen) {
    if (!isOpen) {
      signalPicker.classList.remove("open-up");
      signalPicker.classList.remove("open");
      signalPickerToggle.setAttribute("aria-expanded", "false");
      signalPickerMenu.hidden = true;
      if (deferredSignalPickerRender || deferredSetupRender) {
        deferredSignalPickerRender = false;
        deferredSetupRender = false;
        renderAll();
      }
      return;
    }
    updatePickerDirection(signalPicker, signalPickerMenu);
    signalPicker.classList.toggle("open", isOpen);
    signalPickerToggle.setAttribute("aria-expanded", String(Boolean(isOpen)));
    signalPickerMenu.hidden = !isOpen;
  }

  function setGenPickerOpen(isOpen) {
    if (!haldexGenPicker || !haldexGenToggle || !haldexGenMenu) {
      return;
    }
    if (!isOpen) {
      haldexGenPicker.classList.remove("open-up");
      haldexGenPicker.classList.remove("open");
      haldexGenToggle.setAttribute("aria-expanded", "false");
      haldexGenMenu.hidden = true;
      return;
    }
    updatePickerDirection(haldexGenPicker, haldexGenMenu);
    haldexGenPicker.classList.toggle("open", isOpen);
    haldexGenToggle.setAttribute("aria-expanded", String(Boolean(isOpen)));
    haldexGenMenu.hidden = !isOpen;
  }

  function updatePickerDirection(pickerNode, menuNode) {
    if (!pickerNode || !menuNode) {
      return;
    }
    const wasHidden = menuNode.hidden;
    if (wasHidden) {
      menuNode.hidden = false;
    }
    const rect = pickerNode.getBoundingClientRect();
    const menuHeight = Math.min(menuNode.scrollHeight || 220, 240) + 8;
    const spaceBelow = window.innerHeight - rect.bottom;
    const spaceAbove = rect.top;
    const openUp = spaceBelow < menuHeight && spaceAbove > spaceBelow;
    pickerNode.classList.toggle("open-up", openUp);
    if (wasHidden) {
      menuNode.hidden = true;
    }
  }

  function refreshPickerLabel() {
    const selected = getSignalById(selectedSignalId);
    if (!selected) {
      signalPickerValue.textContent = "Select signal";
      return;
    }
    const value = `${formatValue(selected.value, selected.unit, selected.signal)}${selected.unit ? ` ${selected.unit}` : ""}`;
    signalPickerValue.textContent = `${selected.frame} | ${selected.signal} | ${value}`;
  }

  function renderSignalPicker() {
    if (!decodedSignals.length) {
      signalPickerMenu.innerHTML =
        '<div class="signal-picker-empty">No live decoded signals yet</div>';
      refreshPickerLabel();
      return;
    }

    const items = decodedSignals
      .map((signal) => {
        const selectedClass = signal.key === selectedSignalId ? " is-selected" : "";
        const value = `${formatValue(signal.value, signal.unit, signal.signal)}${signal.unit ? ` ${signal.unit}` : ""}`;
        const busLabel = String(signal.bus || "").toUpperCase();
        return `
          <button type="button" class="signal-option${selectedClass}" data-signal-id="${escapeHtml(signal.key)}" role="option" aria-selected="${signal.key === selectedSignalId}">
            <span class="signal-option-main">${escapeHtml(signal.signal)}</span>
            <span class="signal-option-value">${escapeHtml(value)}</span>
            <span class="signal-option-meta">${escapeHtml(busLabel)} | ${escapeHtml(signal.frame)} | ${escapeHtml(`${signal.hz} Hz`)}</span>
          </button>
        `;
      })
      .join("");

    signalPickerMenu.innerHTML = items;
    refreshPickerLabel();
  }

  function refreshGenPickerLabel() {
    if (!haldexGenValue) {
      return;
    }
    haldexGenValue.textContent = `Gen ${currentHaldexGen}`;
  }

  function renderGenPickerOptions() {
    if (!haldexGenMenu) {
      return;
    }
    haldexGenMenu.querySelectorAll("[data-haldex-gen]").forEach((item) => {
      const active = String(item.getAttribute("data-haldex-gen") || "") === currentHaldexGen;
      item.classList.toggle("is-selected", active);
      item.setAttribute("aria-selected", String(active));
    });
    refreshGenPickerLabel();
  }

  function renderMappedInputs() {
    mappedInputList.innerHTML = requiredInputs
      .map((input) => {
        const active = input.key === selectedInputKey;
        const assigned = mappings[input.key] || "";
        const summary = signalSummary(assigned);
        const activeClass = active ? " is-active" : "";
        const assignedClass = assigned ? " is-assigned" : "";
        return `
          <li class="setup-map-row${activeClass}${assignedClass}" data-input-key="${escapeHtml(input.key)}" role="button" tabindex="0" aria-pressed="${active}">
            <div class="setup-map-main">
              <span class="setup-map-key">${escapeHtml(input.label)}</span>
              <span class="setup-map-value">${escapeHtml(summary.name)}</span>
            </div>
            <span class="setup-live-value">${escapeHtml(summary.value)}</span>
          </li>
        `;
      })
      .join("");
  }

  function renderDashSignals() {
    displaySignalList.innerHTML = dashSlots
      .map((slot) => {
        const active = slot.key === selectedDashKey;
        const assigned = dashMappings[slot.key] || "";
        const summary = signalSummary(assigned);
        const activeClass = active ? " is-active" : "";
        const assignedClass = assigned ? " is-assigned" : "";
        return `
          <li class="setup-map-row${activeClass}${assignedClass}" data-dash-key="${escapeHtml(slot.key)}" role="button" tabindex="0" aria-pressed="${active}">
            <div class="setup-map-main">
              <span class="setup-map-key">${escapeHtml(slot.label)}</span>
              <span class="setup-map-value">${escapeHtml(summary.name)}</span>
            </div>
            <span class="setup-live-value">${escapeHtml(summary.value)}</span>
          </li>
        `;
      })
      .join("");
  }

  function renderAll(options = {}) {
    const skipSignalPicker = Boolean(options.skipSignalPicker);
    if (skipSignalPicker) {
      deferredSignalPickerRender = true;
      deferredSetupRender = true;
      return;
    }
    deferredSignalPickerRender = false;
    deferredSetupRender = false;
    renderSignalPicker();
    renderMappedInputs();
    renderDashSignals();
    const hasMappedInput = requiredInputs.every((input) => Boolean(mappings[input.key]));
    if (saveProfileButton) {
      saveProfileButton.disabled = !hasMappedInput;
    }
  }

  async function loadSignals() {
    if (pollBusy) {
      return;
    }
    pollBusy = true;
    try {
      const payload = await apiJson("/api/canview?decoded=300&raw=0&bus=all");
      decodedSignals = (Array.isArray(payload?.decoded) ? payload.decoded : [])
        .map((item) => formatSignal(item))
        .sort((left, right) => {
          const frameSort = left.frame.localeCompare(right.frame);
          if (frameSort !== 0) {
            return frameSort;
          }
          return left.signal.localeCompare(right.signal);
        });
      signalById = new Map(decodedSignals.map((signal) => [signal.key, signal]));
      if (selectedSignalId && !signalById.has(selectedSignalId)) {
        selectedSignalId = "";
      }
      renderAll({ skipSignalPicker: isSignalPickerOpen() });
    } catch (error) {
      setStatus(`Signal refresh failed: ${error.message}`, true);
    } finally {
      pollBusy = false;
    }
  }

  async function saveSetupToDevice() {
    const payload = {
      haldexGeneration: Number(currentHaldexGen) || 2,
      inputMappings: {
        speed: mappings.speed || "",
        throttle: mappings.throttle || "",
        rpm: mappings.rpm || "",
      },
    };
    try {
      await apiJson("/api/settings", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
      });
      return true;
    } catch (error) {
      setStatus(`Setup save failed: ${error.message}`, true);
      return false;
    }
  }

  function assignSelectedToInput() {
    if (!selectedSignalId) {
      setStatus("Select a signal first.", true);
      return;
    }
    mappings[selectedInputKey] = selectedSignalId;
    renderMappedInputs();
    writeProfile();
    setStatus("Input mapped.");
    renderAll();
  }

  function assignSelectedToDash() {
    if (!selectedSignalId) {
      setStatus("Select a signal first.", true);
      return;
    }
    dashMappings[selectedDashKey] = selectedSignalId;
    renderDashSignals();
    writeProfile();
    setStatus("Dashboard slot mapped.");
    renderAll();
  }

  function clearAllMappings() {
    mappings = { ...defaultMappings };
    dashMappings = { ...defaultDashMappings };
    writeProfile();
    renderAll();
    setStatus("Mappings cleared.");
  }

  async function saveProfile() {
    const hasMappedInput = requiredInputs.every((input) => Boolean(mappings[input.key]));
    if (!hasMappedInput) {
      setStatus("Map Speed, Throttle, and Engine RPM before saving.", true);
      return;
    }
    writeProfile();
    const ok = await saveSetupToDevice();
    if (ok) {
      setStatus("Profile saved to device.");
    }
  }

  async function initFromStoredProfile() {
    const profile = readProfile();
    if (profile?.mappings && typeof profile.mappings === "object") {
      mappings = { ...defaultMappings, ...profile.mappings };
      if (!mappings.rpm && profile.mappings.engine_rpm) {
        mappings.rpm = String(profile.mappings.engine_rpm);
      }
    }
    if (profile?.dashMappings && typeof profile.dashMappings === "object") {
      dashMappings = { ...defaultDashMappings, ...profile.dashMappings };
    }
    const storedGen = String(profile?.haldexGeneration || "");
    if (storedGen === "1" || storedGen === "2" || storedGen === "4") {
      currentHaldexGen = storedGen;
    }

    try {
      const status = await apiJson("/api/status");
      const apiMappings = status?.inputMappings;
      if (apiMappings && typeof apiMappings === "object") {
        mappings = {
          ...mappings,
          speed: String(apiMappings.speed || ""),
          throttle: String(apiMappings.throttle || ""),
          rpm: String(apiMappings.rpm || ""),
        };
      }
      const apiGen = String(status?.haldexGeneration || "");
      if (apiGen === "1" || apiGen === "2" || apiGen === "4") {
        currentHaldexGen = apiGen;
      }
    } catch {
      // Leave local profile values when device status is unavailable.
    }

    renderGenPickerOptions();
  }

  signalPickerToggle.addEventListener("click", () => {
    setPickerOpen(!signalPicker.classList.contains("open"));
  });

  signalPickerMenu.addEventListener("click", (event) => {
    const button = event.target.closest("[data-signal-id]");
    if (!button) {
      return;
    }
    selectedSignalId = String(button.dataset.signalId || "");
    renderSignalPicker();
    setPickerOpen(false);
  });

  if (haldexGenToggle) {
    haldexGenToggle.addEventListener("click", () => {
      setGenPickerOpen(!(haldexGenPicker && haldexGenPicker.classList.contains("open")));
    });
  }

  if (haldexGenMenu) {
    haldexGenMenu.addEventListener("click", (event) => {
      const button = event.target.closest("[data-haldex-gen]");
      if (!button) {
        return;
      }
      const next = String(button.getAttribute("data-haldex-gen") || "");
      if (next !== "1" && next !== "2" && next !== "4") {
        return;
      }
      currentHaldexGen = next;
      writeProfile();
      renderGenPickerOptions();
      setGenPickerOpen(false);
      saveSetupToDevice();
      setStatus(`Generation set: Gen ${currentHaldexGen}`);
    });
  }

  document.addEventListener("click", (event) => {
    if (!signalPicker.contains(event.target)) {
      setPickerOpen(false);
    }
    if (haldexGenPicker && !haldexGenPicker.contains(event.target)) {
      setGenPickerOpen(false);
    }
  });

  mappedInputList.addEventListener("click", (event) => {
    const row = event.target.closest("[data-input-key]");
    if (!row) {
      return;
    }
    selectedInputKey = String(row.dataset.inputKey || selectedInputKey);
    renderMappedInputs();
    setStatus(
      `Target input: ${requiredInputs.find((item) => item.key === selectedInputKey)?.label || selectedInputKey}`
    );
  });

  mappedInputList.addEventListener("keydown", (event) => {
    if (event.key !== "Enter" && event.key !== " ") {
      return;
    }
    const row = event.target.closest("[data-input-key]");
    if (!row) {
      return;
    }
    event.preventDefault();
    selectedInputKey = String(row.dataset.inputKey || selectedInputKey);
    renderMappedInputs();
  });

  displaySignalList.addEventListener("click", (event) => {
    const row = event.target.closest("[data-dash-key]");
    if (!row) {
      return;
    }
    selectedDashKey = String(row.dataset.dashKey || selectedDashKey);
    renderDashSignals();
    setStatus(
      `Target dashboard slot: ${dashSlots.find((slot) => slot.key === selectedDashKey)?.label || selectedDashKey}`
    );
  });

  displaySignalList.addEventListener("keydown", (event) => {
    if (event.key !== "Enter" && event.key !== " ") {
      return;
    }
    const row = event.target.closest("[data-dash-key]");
    if (!row) {
      return;
    }
    event.preventDefault();
    selectedDashKey = String(row.dataset.dashKey || selectedDashKey);
    renderDashSignals();
  });

  if (mapSelectedButton) {
    mapSelectedButton.addEventListener("click", assignSelectedToInput);
  }
  if (addDashButton) {
    addDashButton.addEventListener("click", assignSelectedToDash);
  }
  if (clearMappingsButton) {
    clearMappingsButton.addEventListener("click", clearAllMappings);
  }
  if (saveProfileButton) {
    saveProfileButton.addEventListener("click", saveProfile);
  }

  initFromStoredProfile().finally(() => {
    renderAll();
    setStatus("Click a target row, pick a signal, then assign.", true);

    loadSignals();
    pollTimer = window.setInterval(loadSignals, 1000);
    window.addEventListener("beforeunload", () => {
      if (pollTimer) {
        window.clearInterval(pollTimer);
      }
    });
  });
}

function initCurvePage() {
  const page = String(document.body?.dataset?.page || "").toLowerCase();
  if (page !== "speed" && page !== "throttle" && page !== "rpm") {
    return;
  }

  const card = document.querySelector(".curve-card");
  const rowsNode = document.getElementById("curveRows");
  const statusNode = document.getElementById("curveStatus");
  const liveNode = document.getElementById("curveLiveValue");
  const btnLoad = document.getElementById("curveLoad");
  const btnAdd = document.getElementById("curveAdd");
  const btnReset = document.getElementById("curveReset");
  const btnSave = document.getElementById("curveSave");
  const disengageInput = document.getElementById("curveDisengageSpeed");
  const disengageToggle = document.getElementById("curveEnableDisengageSpeed");
  const btnDisengageSave = document.getElementById("curveDisengageSave");
  const throttleGateInput = document.getElementById("curveDisableThrottle");
  const throttleGateToggle = document.getElementById("curveEnableThrottleGate");
  const speedGateInput = document.getElementById("curveDisableSpeed");
  const speedGateToggle = document.getElementById("curveEnableSpeedGate");
  const releaseRateInput = document.getElementById("curveReleaseRate");
  const releaseRateToggle = document.getElementById("curveEnableReleaseRate");
  const broadcastToggle = document.getElementById("curveBroadcastBridge");
  const controllerToggle = document.getElementById("curveControllerEnabled");
  if (!card || !rowsNode || !statusNode) {
    return;
  }

  const isSpeed = page === "speed";
  const isThrottle = page === "throttle";
  const isRpm = page === "rpm";
  const config = {
    endpoint: isSpeed ? "/api/curve/speed" : isThrottle ? "/api/curve/throttle" : "/api/curve/rpm",
    modeName: isSpeed ? "speed" : isThrottle ? "throttle" : "rpm",
    xLabel: isSpeed ? "km/h" : isThrottle ? "%" : "rpm",
    xMin: 0,
    xMax: isSpeed ? 300 : isThrottle ? 100 : 10000,
    xStep: 1,
    disengageKey: isSpeed ? "speed" : isThrottle ? "throttle" : "rpm",
    liveLabel: isSpeed ? "Speed" : isThrottle ? "Throttle" : "RPM",
    liveKey: isSpeed ? "speed" : isThrottle ? "throttle" : "rpm",
    defaults: isSpeed
      ? [
          { x: 0, lock: 50 },
          { x: 20, lock: 45 },
          { x: 40, lock: 35 },
          { x: 80, lock: 20 },
          { x: 140, lock: 0 },
        ]
      : isThrottle
        ? [
            { x: 0, lock: 0 },
            { x: 10, lock: 10 },
            { x: 25, lock: 25 },
            { x: 50, lock: 55 },
            { x: 80, lock: 80 },
          ]
        : [
            { x: 0, lock: 0 },
            { x: 1000, lock: 10 },
            { x: 2000, lock: 30 },
            { x: 3500, lock: 55 },
            { x: 5000, lock: 80 },
            { x: 6500, lock: 100 },
          ],
  };

  const cacheKeys = {
    disengage: `dynamic:disengage:${config.disengageKey}`,
    throttleGate: "dynamic:global:disableThrottle",
    speedGate: "dynamic:global:disableSpeed",
    releaseRate: "dynamic:global:releaseRate",
  };

  let points = config.defaults.map((point) => ({ ...point }));
  let pollTimer = null;

  const clampInt = (value, min, max) => {
    const n = Number(value);
    if (!Number.isFinite(n)) {
      return min;
    }
    return Math.max(min, Math.min(max, Math.round(n)));
  };

  const setStatus = (message, pending = false) => {
    statusNode.textContent = message;
    statusNode.classList.toggle("pending", Boolean(pending));
  };

  const clampDisengageSpeed = (value) => clampInt(value, 0, 300);
  const clampThrottleGate = (value) => clampInt(value, 0, 100);
  const clampSpeedGate = (value) => clampInt(value, 0, 300);
  const clampReleaseRate = (value) => clampInt(value, 0, 1000);
  const readToggleChecked = (toggle, fallback = true) =>
    toggle ? Boolean(toggle.checked) : fallback;

  const applySettingFromStatus = ({
    apiValue,
    clamp,
    input,
    toggle,
    cacheKey,
    fallbackValue = 0,
  }) => {
    const effectiveApi = clamp(apiValue ?? 0);
    const cached = clamp(getModeBehaviorValue(cacheKey, fallbackValue));
    const enabled = effectiveApi > 0;
    const value = enabled ? effectiveApi : cached;
    if (input) {
      input.value = String(value);
    }
    if (toggle) {
      toggle.checked = enabled;
    }
    setModeBehaviorValue(cacheKey, value);
    return { enabled, value };
  };

  const readDisengageSpeed = () => {
    if (!disengageInput) {
      return 0;
    }
    const value = clampDisengageSpeed(disengageInput.value);
    disengageInput.value = String(value);
    setModeBehaviorValue(cacheKeys.disengage, value);
    return value;
  };

  const readThrottleGate = () => {
    if (!throttleGateInput) {
      return 0;
    }
    const value = clampThrottleGate(throttleGateInput.value);
    throttleGateInput.value = String(value);
    setModeBehaviorValue(cacheKeys.throttleGate, value);
    return value;
  };

  const readSpeedGate = () => {
    if (!speedGateInput) {
      return 0;
    }
    const value = clampSpeedGate(speedGateInput.value);
    speedGateInput.value = String(value);
    setModeBehaviorValue(cacheKeys.speedGate, value);
    return value;
  };

  const readReleaseRate = () => {
    if (!releaseRateInput) {
      return 120;
    }
    const value = clampReleaseRate(releaseRateInput.value);
    releaseRateInput.value = String(value);
    setModeBehaviorValue(cacheKeys.releaseRate, value);
    return value;
  };

  const modeSettingsSummary = (settings) => {
    const disengageSummary =
      settings.disengageEnabled && settings.disengageSpeed > 0
        ? `Disengage below ${settings.disengageSpeed} km/h.`
        : settings.disengageSpeed > 0
          ? `Disengage gate off (saved ${settings.disengageSpeed} km/h).`
          : "Disengage gate off.";
    const throttleSummary =
      settings.disableThrottleEnabled && settings.disableThrottle > 0
        ? `Throttle >= ${settings.disableThrottle}%.`
        : settings.disableThrottle > 0
          ? `Throttle gate off (saved ${settings.disableThrottle}%).`
          : "Throttle gate off.";
    const speedSummary =
      settings.disableSpeedEnabled && settings.disableSpeed > 0
        ? `Disable above ${settings.disableSpeed} km/h.`
        : settings.disableSpeed > 0
          ? `High-speed gate off (saved ${settings.disableSpeed} km/h).`
          : "High-speed gate off.";
    const releaseSummary =
      settings.releaseRateEnabled && settings.releaseRate > 0
        ? `Release ramp ${settings.releaseRate} %/s.`
        : settings.releaseRate > 0
          ? `Release ramp off (saved ${settings.releaseRate} %/s).`
          : "Release ramp off.";
    const broadcastSummary = settings.broadcastOpenHaldexOverCAN ? "Bridge on." : "Bridge off.";
    const controllerSummary = settings.disableController ? "Controller off." : "Controller on.";
    return `${disengageSummary} ${throttleSummary} ${speedSummary} ${releaseSummary} ${broadcastSummary} ${controllerSummary}`;
  };

  const readModeSettingsState = () => {
    const disengageSpeed = readDisengageSpeed();
    const disableThrottle = readThrottleGate();
    const disableSpeed = readSpeedGate();
    const releaseRate = readReleaseRate();
    return {
      disengageSpeed,
      disengageEnabled: readToggleChecked(disengageToggle, disengageSpeed > 0),
      disableThrottle,
      disableThrottleEnabled: readToggleChecked(throttleGateToggle, disableThrottle > 0),
      disableSpeed,
      disableSpeedEnabled: readToggleChecked(speedGateToggle, disableSpeed > 0),
      releaseRate,
      releaseRateEnabled: readToggleChecked(releaseRateToggle, releaseRate > 0),
      broadcastOpenHaldexOverCAN: broadcastToggle ? Boolean(broadcastToggle.checked) : true,
      disableController: controllerToggle ? !Boolean(controllerToggle.checked) : false,
    };
  };

  const readModeSettingsPayload = () => {
    const state = readModeSettingsState();
    return {
      state,
      payload: {
        disableThrottle: state.disableThrottleEnabled ? state.disableThrottle : 0,
        disableSpeed: state.disableSpeedEnabled ? state.disableSpeed : 0,
        broadcastOpenHaldexOverCAN: state.broadcastOpenHaldexOverCAN,
        disableController: state.disableController,
        lockReleaseRatePctPerSec: state.releaseRateEnabled ? state.releaseRate : 0,
        disengageUnderSpeed: {
          [config.disengageKey]: state.disengageEnabled ? state.disengageSpeed : 0,
        },
      },
    };
  };

  const applyModeSettingsFromStatus = (status) => {
    const disengage = applySettingFromStatus({
      apiValue: status?.disengageUnderSpeed?.[config.disengageKey],
      clamp: clampDisengageSpeed,
      input: disengageInput,
      toggle: disengageToggle,
      cacheKey: cacheKeys.disengage,
      fallbackValue: 0,
    });
    const throttleGate = applySettingFromStatus({
      apiValue: status?.disableThrottle,
      clamp: clampThrottleGate,
      input: throttleGateInput,
      toggle: throttleGateToggle,
      cacheKey: cacheKeys.throttleGate,
      fallbackValue: 0,
    });
    const speedGate = applySettingFromStatus({
      apiValue: status?.disableSpeed,
      clamp: clampSpeedGate,
      input: speedGateInput,
      toggle: speedGateToggle,
      cacheKey: cacheKeys.speedGate,
      fallbackValue: 0,
    });
    const releaseRate = applySettingFromStatus({
      apiValue: status?.lockReleaseRatePctPerSec ?? 120,
      clamp: clampReleaseRate,
      input: releaseRateInput,
      toggle: releaseRateToggle,
      cacheKey: cacheKeys.releaseRate,
      fallbackValue: 120,
    });
    if (broadcastToggle) {
      broadcastToggle.checked = status?.broadcastOpenHaldexOverCAN !== false;
    }
    if (controllerToggle) {
      controllerToggle.checked = !Boolean(status?.disableController);
    }
    return {
      disengageSpeed: disengage.value,
      disengageEnabled: disengage.enabled,
      disableThrottle: throttleGate.value,
      disableThrottleEnabled: throttleGate.enabled,
      disableSpeed: speedGate.value,
      disableSpeedEnabled: speedGate.enabled,
      broadcastOpenHaldexOverCAN: broadcastToggle ? Boolean(broadcastToggle.checked) : true,
      disableController: controllerToggle ? !Boolean(controllerToggle.checked) : false,
      releaseRate: releaseRate.value,
      releaseRateEnabled: releaseRate.enabled,
    };
  };

  const loadModeSettings = async () => {
    const status = await apiJson("/api/status");
    return applyModeSettingsFromStatus(status);
  };

  const saveModeSettings = async () => {
    const { payload, state } = readModeSettingsPayload();
    await apiJson("/api/settings", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
    try {
      return await loadModeSettings();
    } catch {
      return {
        disengageSpeed: state.disengageSpeed,
        disengageEnabled: state.disengageEnabled,
        disableThrottle: state.disableThrottle,
        disableThrottleEnabled: state.disableThrottleEnabled,
        disableSpeed: state.disableSpeed,
        disableSpeedEnabled: state.disableSpeedEnabled,
        broadcastOpenHaldexOverCAN: state.broadcastOpenHaldexOverCAN,
        disableController: state.disableController,
        releaseRate: state.releaseRate,
        releaseRateEnabled: state.releaseRateEnabled,
      };
    }
  };

  const normalizePoints = (candidate) => {
    const normalized = candidate
      .map((point) => ({
        x: clampInt(point.x, config.xMin, config.xMax),
        lock: clampInt(point.lock, 0, 100),
      }))
      .sort((left, right) => left.x - right.x);

    if (!normalized.length) {
      throw new Error("Add at least one point");
    }
    if (normalized.length > 12) {
      throw new Error("Max 12 points");
    }

    for (let i = 1; i < normalized.length; i++) {
      if (normalized[i].x <= normalized[i - 1].x) {
        throw new Error("X values must strictly increase");
      }
    }
    return normalized;
  };

  const getNextPointX = () => {
    if (!points.length) {
      return config.xMin;
    }
    const used = new Set(points.map((point) => point.x));
    let x = Math.min(
      config.xMax,
      points[points.length - 1].x + (isSpeed ? 10 : isThrottle ? 5 : 250)
    );
    while (used.has(x) && x < config.xMax) {
      x += config.xStep;
    }
    if (used.has(x)) {
      for (let probe = config.xMin; probe <= config.xMax; probe += config.xStep) {
        if (!used.has(probe)) {
          return probe;
        }
      }
      return -1;
    }
    return x;
  };

  const rowMarkup = (point, index) => {
    return `
      <div class="curve-row" role="listitem" data-index="${index}">
        <label class="curve-x-cell">
          <input
            type="number"
            class="curve-x-input"
            data-action="x"
            min="${config.xMin}"
            max="${config.xMax}"
            step="${config.xStep}"
            value="${point.x}"
            aria-label="Point ${index + 1} x value ${config.xLabel}" />
          <span class="curve-unit">${config.xLabel}</span>
        </label>
        <div class="curve-lock-cell">
          <input
            type="range"
            class="curve-lock-slider"
            data-action="lock-slider"
            min="0"
            max="100"
            step="1"
            value="${point.lock}"
            aria-label="Point ${index + 1} lock slider" />
          <input
            type="number"
            class="curve-lock-input"
            data-action="lock-input"
            min="0"
            max="100"
            step="1"
            value="${point.lock}"
            aria-label="Point ${index + 1} lock value" />
          <span class="curve-unit">%</span>
        </div>
        <button type="button" class="curve-remove" data-action="remove" aria-label="Remove point ${index + 1}">Remove</button>
      </div>
    `;
  };

  const render = () => {
    rowsNode.innerHTML = points.map((point, index) => rowMarkup(point, index)).join("");
    rowsNode.querySelectorAll('[data-action="remove"]').forEach((button) => {
      button.disabled = points.length <= 1;
    });
  };

  const fetchCurve = async () => {
    setStatus("Loading curve...", true);
    const data = await apiJson(config.endpoint);
    const apiPoints = Array.isArray(data?.points) ? data.points : [];
    points = normalizePoints(
      apiPoints.map((point) => ({
        x: point?.x,
        lock: point?.lock,
      }))
    );
    let settings = null;
    try {
      settings = await loadModeSettings();
    } catch {
      // Keep curve loading resilient when settings endpoint is unavailable.
    }
    render();
    if (settings) {
      setStatus(`Loaded ${points.length} points. ${modeSettingsSummary(settings)}`);
    } else {
      setStatus(`Loaded ${points.length} points.`);
    }
  };

  const saveCurve = async () => {
    let normalized;
    try {
      normalized = normalizePoints(points);
      points = normalized;
    } catch (error) {
      setStatus(error.message, true);
      return;
    }

    setStatus("Saving curve...", true);
    await apiJson(config.endpoint, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ points: normalized }),
    });

    const settings = await saveModeSettings();
    setStatus(`Saved ${normalized.length} points. ${modeSettingsSummary(settings)}`);
    render();
  };

  const updateLiveValue = async () => {
    if (!liveNode) {
      return;
    }
    try {
      const status = await apiJson("/api/status");
      const telemetry = status?.telemetry || {};
      const value = telemetry[config.liveKey];
      const formatted = formatValue(value);
      liveNode.textContent = `${config.liveLabel}: ${formatted} ${config.xLabel}`;
    } catch {
      liveNode.textContent = `${config.liveLabel}: -- ${config.xLabel}`;
    }
  };

  rowsNode.addEventListener("input", (event) => {
    const target = event.target;
    const row = target.closest(".curve-row");
    if (!row) {
      return;
    }
    const index = Number(row.dataset.index);
    if (!Number.isInteger(index) || index < 0 || index >= points.length) {
      return;
    }

    const action = target.dataset.action;
    if (action === "x") {
      points[index].x = clampInt(target.value, config.xMin, config.xMax);
      target.value = String(points[index].x);
      return;
    }
    if (action === "lock-slider") {
      points[index].lock = clampInt(target.value, 0, 100);
      const lockInput = row.querySelector('[data-action="lock-input"]');
      if (lockInput) {
        lockInput.value = String(points[index].lock);
      }
      return;
    }
    if (action === "lock-input") {
      points[index].lock = clampInt(target.value, 0, 100);
      target.value = String(points[index].lock);
      const lockSlider = row.querySelector('[data-action="lock-slider"]');
      if (lockSlider) {
        lockSlider.value = String(points[index].lock);
      }
    }
  });

  rowsNode.addEventListener("click", (event) => {
    const target = event.target.closest('[data-action="remove"]');
    if (!target) {
      return;
    }
    const row = target.closest(".curve-row");
    if (!row) {
      return;
    }
    const index = Number(row.dataset.index);
    if (!Number.isInteger(index) || index < 0 || index >= points.length) {
      return;
    }
    if (points.length <= 1) {
      setStatus("At least one point is required.", true);
      return;
    }
    points.splice(index, 1);
    render();
    setStatus("Point removed.");
  });

  if (disengageInput) {
    disengageInput.addEventListener("change", () => {
      readDisengageSpeed();
    });
  }
  if (throttleGateInput) {
    throttleGateInput.addEventListener("change", () => {
      readThrottleGate();
    });
  }
  if (speedGateInput) {
    speedGateInput.addEventListener("change", () => {
      readSpeedGate();
    });
  }
  if (releaseRateInput) {
    releaseRateInput.addEventListener("change", () => {
      readReleaseRate();
    });
  }

  if (btnDisengageSave) {
    btnDisengageSave.addEventListener("click", async () => {
      try {
        setStatus("Saving mode settings...", true);
        const settings = await saveModeSettings();
        setStatus(`Saved mode settings. ${modeSettingsSummary(settings)}`);
      } catch (error) {
        setStatus(`Mode settings save failed: ${error.message}`, true);
      }
    });
  }

  if (btnLoad) {
    btnLoad.addEventListener("click", async () => {
      try {
        await fetchCurve();
      } catch (error) {
        setStatus(`Load failed: ${error.message}`, true);
      }
    });
  }

  if (btnAdd) {
    btnAdd.addEventListener("click", () => {
      if (points.length >= 12) {
        setStatus("Max 12 points reached.", true);
        return;
      }
      const x = getNextPointX();
      if (x < 0) {
        setStatus("No available x slot left.", true);
        return;
      }
      const lock = points.length ? points[points.length - 1].lock : 0;
      points.push({ x, lock });
      points = points.sort((left, right) => left.x - right.x);
      render();
      setStatus("Point added.");
    });
  }

  if (btnReset) {
    btnReset.addEventListener("click", () => {
      points = config.defaults.map((point) => ({ ...point }));
      render();
      setStatus("Defaults loaded locally. Save to apply.");
    });
  }

  if (btnSave) {
    btnSave.addEventListener("click", async () => {
      try {
        await saveCurve();
      } catch (error) {
        setStatus(`Save failed: ${error.message}`, true);
      }
    });
  }

  render();
  fetchCurve().catch((error) => {
    setStatus(`Load failed: ${error.message}`, true);
  });
  updateLiveValue();
  pollTimer = window.setInterval(updateLiveValue, 1000);
  window.addEventListener("beforeunload", () => {
    if (pollTimer) {
      window.clearInterval(pollTimer);
    }
  });
}

function initLogsPage() {
  const output = document.getElementById("logOutput");
  if (!output) {
    return;
  }

  const logStatus = document.getElementById("logStatus");
  const logSettingsStatus = document.getElementById("logSettingsStatus");
  const logFileStatus = document.getElementById("logFileStatus");
  const masterToggle = document.getElementById("logEnableMaster");
  const canToggle = document.getElementById("logEnableCan");
  const errorToggle = document.getElementById("logEnableError");
  const serialToggle = document.getElementById("logEnableSerial");
  const firmwareToggle = document.getElementById("logDebugFirmware");
  const networkToggle = document.getElementById("logDebugNetwork");
  const canDebugToggle = document.getElementById("logDebugCan");
  const fileSelect = document.getElementById("logFileSelect");
  const scopeSelect = document.getElementById("logScopeSelect");
  const btnApply = document.getElementById("btnLogApply");
  const btnPause = document.getElementById("btnLogPause");
  const btnClearView = document.getElementById("btnLogClear");
  const btnDownload = document.getElementById("btnLogDownload");
  const btnRefreshList = document.getElementById("btnLogRefreshList");
  const btnDelete = document.getElementById("btnLogDelete");
  const btnClearScope = document.getElementById("btnLogClearScope");

  let paused = false;
  let activePath = "";
  let polling = false;
  let pollTimer = null;
  let pollCount = 0;

  function setStatus(message) {
    if (logStatus) {
      logStatus.textContent = message;
    }
  }

  function setSettingsStatus(message) {
    if (logSettingsStatus) {
      logSettingsStatus.textContent = message;
    }
  }

  function setFileStatus(message) {
    if (logFileStatus) {
      logFileStatus.textContent = message;
    }
  }

  function logNameFromPath(path) {
    const value = String(path || "");
    return value.split("/").pop() || value;
  }

  function syncToggleState() {
    if (
      !masterToggle ||
      !canToggle ||
      !errorToggle ||
      !serialToggle ||
      !firmwareToggle ||
      !networkToggle ||
      !canDebugToggle
    ) {
      return;
    }
    const on = Boolean(masterToggle.checked);
    canToggle.disabled = !on;
    errorToggle.disabled = !on;
    serialToggle.disabled = !on;
    firmwareToggle.disabled = !on;
    networkToggle.disabled = !on;
    canDebugToggle.disabled = !on;
  }

  async function refreshSettings() {
    if (
      !masterToggle ||
      !canToggle ||
      !errorToggle ||
      !serialToggle ||
      !firmwareToggle ||
      !networkToggle ||
      !canDebugToggle
    ) {
      return;
    }
    setSettingsStatus("Loading...");
    try {
      const status = await apiJson("/api/status");
      const logging = status.logging || {};
      masterToggle.checked = Boolean(logging.masterEnabled);
      canToggle.checked = Boolean(logging.canEnabled);
      errorToggle.checked = Boolean(logging.errorEnabled);
      serialToggle.checked = Boolean(logging.serialEnabled);
      firmwareToggle.checked = Boolean(logging.debugFirmwareEnabled);
      networkToggle.checked = Boolean(logging.debugNetworkEnabled);
      canDebugToggle.checked = Boolean(logging.debugCanEnabled);
      syncToggleState();
      setSettingsStatus(
        logging.debugCaptureActive ? "Loaded (debug capture active: STOCK mode forced)" : "Loaded"
      );
    } catch (error) {
      setSettingsStatus(`Load failed: ${error.message}`);
    }
  }

  async function applySettings() {
    if (
      !masterToggle ||
      !canToggle ||
      !errorToggle ||
      !serialToggle ||
      !firmwareToggle ||
      !networkToggle ||
      !canDebugToggle
    ) {
      return;
    }
    setSettingsStatus("Saving...");
    try {
      const master = Boolean(masterToggle.checked);
      const payload = master
        ? {
            logToFileEnabled: true,
            logCanToFileEnabled: Boolean(canToggle.checked),
            logErrorToFileEnabled: Boolean(errorToggle.checked),
            logSerialEnabled: Boolean(serialToggle.checked),
            logDebugFirmwareEnabled: Boolean(firmwareToggle.checked),
            logDebugNetworkEnabled: Boolean(networkToggle.checked),
            logDebugCanEnabled: Boolean(canDebugToggle.checked),
          }
        : {
            logToFileEnabled: false,
            logCanToFileEnabled: false,
            logErrorToFileEnabled: false,
            logSerialEnabled: false,
            logDebugFirmwareEnabled: false,
            logDebugNetworkEnabled: false,
            logDebugCanEnabled: false,
          };

      const response = await apiJson("/api/settings", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
      });
      if (!master) {
        canToggle.checked = false;
        errorToggle.checked = false;
        serialToggle.checked = false;
        firmwareToggle.checked = false;
        networkToggle.checked = false;
        canDebugToggle.checked = false;
      }
      syncToggleState();
      const forcedStock = Boolean(response?.debugCaptureActive);
      setSettingsStatus(forcedStock ? "Saved (debug capture active: STOCK mode forced)" : "Saved");
    } catch (error) {
      setSettingsStatus(`Save failed: ${error.message}`);
    }
  }

  async function refreshFileList(preferredPath = "") {
    if (!fileSelect) {
      return;
    }

    const previous = preferredPath || activePath || fileSelect.value || "";
    setFileStatus("Loading...");

    try {
      const data = await apiJson("/api/logs");
      const files = Array.isArray(data.files) ? data.files.slice() : [];
      files.sort((a, b) => String(a.path || "").localeCompare(String(b.path || "")));

      fileSelect.innerHTML = "";

      if (!files.length) {
        const placeholder = document.createElement("option");
        placeholder.value = "";
        placeholder.textContent = "No .txt log files yet";
        fileSelect.appendChild(placeholder);
        fileSelect.disabled = true;
        activePath = "";
        output.textContent = "No logs yet. Enable log capture, drive briefly, then refresh.";
        setFileStatus("No .txt log files yet");
        return;
      }

      files.forEach((entry) => {
        const option = document.createElement("option");
        option.value = entry.path || "";
        const name = logNameFromPath(entry.path || "");
        option.textContent = `${name} (${entry.scope || "log"}, ${formatBytes(entry.size)})`;
        fileSelect.appendChild(option);
      });

      const exists = files.some((entry) => entry.path === previous);
      activePath = exists ? previous : files[0].path;
      fileSelect.value = activePath;
      fileSelect.disabled = false;
      setFileStatus(`${files.length} file(s)`);
    } catch (error) {
      setFileStatus(`List failed: ${error.message}`);
    }
  }

  async function loadActiveFile() {
    if (!activePath) {
      output.textContent = "No logs available yet.";
      setStatus("Idle");
      return;
    }
    try {
      const text = await apiText(`/api/logs/read?path=${encodeURIComponent(activePath)}&max=65536`);
      const trimmed = String(text || "").trim();
      if (trimmed.startsWith("{") && trimmed.includes('"files"')) {
        setStatus("Read endpoint returned file-list JSON; refreshing...");
        await refreshFileList(activePath);
        return;
      }
      output.textContent = text || "[empty file]";
      if (!paused) {
        output.scrollTop = output.scrollHeight;
      }
      setStatus(`Streaming ${new Date().toLocaleTimeString()}`);
    } catch (error) {
      const msg = String(error?.message || "read failed");
      if (msg.includes("404") || msg.includes("log read failed")) {
        await refreshFileList("");
        if (!activePath) {
          output.textContent = "No logs yet. Enable log capture, drive briefly, then refresh.";
          setStatus("Idle");
          return;
        }
      }
      setStatus(`Read failed: ${msg}`);
    }
  }

  async function deleteActiveFile() {
    if (!activePath) {
      return;
    }
    setFileStatus("Deleting...");
    try {
      await apiJson("/api/logs/delete", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ path: activePath }),
      });
      await refreshFileList("");
      await loadActiveFile();
      setFileStatus("Deleted");
    } catch (error) {
      setFileStatus(`Delete failed: ${error.message}`);
    }
  }

  async function clearScope() {
    if (!scopeSelect) {
      return;
    }
    const scope = scopeSelect.value || "everything";
    setFileStatus("Clearing...");
    try {
      await apiJson("/api/logs/clear", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ scope }),
      });
      await refreshFileList("");
      await loadActiveFile();
      setFileStatus("Cleared");
    } catch (error) {
      setFileStatus(`Clear failed: ${error.message}`);
    }
  }

  function togglePause() {
    paused = !paused;
    if (btnPause) {
      btnPause.textContent = paused ? "Resume" : "Pause";
    }
    setStatus(paused ? "Paused" : "Streaming");
    if (!paused) {
      loadActiveFile();
    }
  }

  function clearView() {
    output.textContent = "";
    setStatus(paused ? "Paused" : "View cleared");
  }

  function downloadView() {
    const body = output.textContent || "";
    const blob = new Blob([body], { type: "text/plain;charset=utf-8" });
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    const name = activePath ? activePath.split("/").pop() : "openhaldex-log.txt";
    link.href = url;
    link.download = sanitizeFilename(name || "openhaldex-log.txt");
    document.body.appendChild(link);
    link.click();
    link.remove();
    URL.revokeObjectURL(url);
  }

  async function poll() {
    if (paused || polling) {
      return;
    }
    polling = true;
    pollCount += 1;
    try {
      if (pollCount % 5 === 1) {
        await refreshFileList(activePath);
      }
      await loadActiveFile();
    } finally {
      polling = false;
    }
  }

  if (btnApply) {
    btnApply.addEventListener("click", applySettings);
  }
  if (masterToggle) {
    masterToggle.addEventListener("change", syncToggleState);
  }
  if (fileSelect) {
    fileSelect.addEventListener("change", () => {
      activePath = fileSelect.value || "";
      loadActiveFile();
    });
  }
  if (btnRefreshList) {
    btnRefreshList.addEventListener("click", async () => {
      await refreshFileList(activePath);
      await loadActiveFile();
    });
  }
  if (btnDelete) {
    btnDelete.addEventListener("click", deleteActiveFile);
  }
  if (btnClearScope) {
    btnClearScope.addEventListener("click", clearScope);
  }
  if (btnPause) {
    btnPause.addEventListener("click", togglePause);
  }
  if (btnClearView) {
    btnClearView.addEventListener("click", clearView);
  }
  if (btnDownload) {
    btnDownload.addEventListener("click", downloadView);
  }

  refreshSettings();
  refreshFileList("").then(loadActiveFile);
  pollTimer = window.setInterval(poll, 3000);
  window.addEventListener("beforeunload", () => {
    if (pollTimer) {
      window.clearInterval(pollTimer);
    }
  });
}

function getPageKey() {
  const page = document.body?.dataset?.page;
  if (page) {
    return page;
  }
  if (document.querySelector(".logs-main")) {
    return "logs";
  }
  return "default";
}

const MODE_BEHAVIOR_CACHE_KEY = "ohModeBehaviorValues:v1";

function readModeBehaviorCache() {
  try {
    const raw = localStorage.getItem(MODE_BEHAVIOR_CACHE_KEY);
    if (!raw) {
      return {};
    }
    const parsed = JSON.parse(raw);
    if (!parsed || typeof parsed !== "object") {
      return {};
    }
    return parsed;
  } catch {
    return {};
  }
}

function writeModeBehaviorCache(cache) {
  try {
    localStorage.setItem(MODE_BEHAVIOR_CACHE_KEY, JSON.stringify(cache));
  } catch {
    // Ignore storage failures.
  }
}

function getModeBehaviorValue(cacheKey, fallback = 0) {
  const cache = readModeBehaviorCache();
  const value = cache?.[cacheKey];
  const numeric = Number(value);
  return Number.isFinite(numeric) ? numeric : fallback;
}

function setModeBehaviorValue(cacheKey, value) {
  const numeric = Number(value);
  if (!Number.isFinite(numeric)) {
    return;
  }
  const cache = readModeBehaviorCache();
  cache[cacheKey] = numeric;
  writeModeBehaviorCache(cache);
}

function initDetailsPersistence() {
  const pageKey = getPageKey();
  const details = document.querySelectorAll("details[data-detail]");
  details.forEach((item) => {
    const detailId = item.dataset.detail;
    if (!detailId) {
      return;
    }
    const storageKey = `ohDetails:${pageKey}:${detailId}`;
    try {
      const saved = localStorage.getItem(storageKey);
      if (saved === "open") {
        item.open = true;
      } else if (saved === "closed") {
        item.open = false;
      }
    } catch {
      // Ignore storage failures.
    }
    item.addEventListener("toggle", () => {
      try {
        localStorage.setItem(storageKey, item.open ? "open" : "closed");
      } catch {
        // Ignore storage failures.
      }
    });
  });
}

function initCollapsibleCards() {
  const pageKey = getPageKey();
  const targets = [];
  if (document.querySelector(".canview-main")) {
    targets.push(".canview-main");
  }
  if (document.querySelector(".diag-main")) {
    targets.push(".diag-main");
  }
  if (document.querySelector(".logs-main")) {
    targets.push(".logs-main");
  }
  if (!targets.length) {
    return;
  }
  const cards = [];
  targets.forEach((selector) => {
    document.querySelectorAll(`${selector} .ui-card`).forEach((card) => cards.push(card));
  });
  cards.forEach((card, index) => {
    const header = card.querySelector(".ui-section-head");
    if (!header) {
      return;
    }
    const title =
      header.querySelector("h2")?.textContent?.trim() ||
      header.textContent?.trim() ||
      `card-${index}`;
    const slug =
      title
        .toLowerCase()
        .replace(/[^a-z0-9]+/g, "-")
        .replace(/^-+|-+$/g, "") || `card-${index}`;
    const storageKey = `ohCard:${pageKey}:${slug}`;
    card.classList.add("collapsible");

    let collapsed = true;
    try {
      const saved = localStorage.getItem(storageKey);
      if (saved === "open") {
        collapsed = false;
      } else if (saved === "collapsed") {
        collapsed = true;
      }
    } catch {
      // Ignore storage failures.
    }
    card.classList.toggle("is-collapsed", collapsed);

    header.setAttribute("role", "button");
    header.tabIndex = 0;
    const toggle = () => {
      const next = !card.classList.contains("is-collapsed");
      card.classList.toggle("is-collapsed", next);
      try {
        localStorage.setItem(storageKey, next ? "collapsed" : "open");
      } catch {
        // Ignore storage failures.
      }
    };
    header.addEventListener("click", toggle);
    header.addEventListener("keydown", (event) => {
      if (event.key === "Enter" || event.key === " ") {
        event.preventDefault();
        toggle();
      }
    });
  });
}

if (menuToggle) {
  menuToggle.addEventListener("click", () => {
    const isOpen = !slideMenu?.classList.contains("active");
    setMenuState(Boolean(isOpen));
  });
}

if (menuClose) {
  menuClose.addEventListener("click", () => {
    setMenuState(false);
  });
}

if (menuBackdrop) {
  menuBackdrop.addEventListener("click", () => {
    setMenuState(false);
  });
}

menuItems.forEach((item) => {
  item.addEventListener("click", (event) => {
    const href = item.getAttribute("href") || "";
    if (href.startsWith("#")) {
      event.preventDefault();
    }
    setMenuState(false);
  });
});

if (themeToggle) {
  themeToggle.addEventListener("click", toggleTheme);
}

if (fullscreenToggle) {
  fullscreenToggle.addEventListener("click", () => {
    if (!document.fullscreenElement) {
      document.documentElement.requestFullscreen().catch((err) => {
        console.log("Fullscreen request failed:", err);
      });
    } else {
      document.exitFullscreen();
    }
  });
}

document.addEventListener("fullscreenchange", updateFullscreenLabel);

document.addEventListener("keydown", (event) => {
  if (event.key === "Escape" && slideMenu?.classList.contains("active")) {
    setMenuState(false);
  }
});

document.addEventListener("keydown", (event) => {
  if (event.defaultPrevented || event.ctrlKey || event.metaKey || event.altKey || event.shiftKey) {
    return;
  }

  if (isEditableTarget(event.target)) {
    return;
  }

  if (event.key.toLowerCase() === "t") {
    toggleTheme();
  }
});

buttons.forEach((button) => {
  button.addEventListener("click", async () => {
    setMode(button);
    try {
      await pushHomeMode(button);
    } catch {
      // Status poll will reconcile UI if write fails.
    }
  });
});

if (ratioSlider && frontNumber && rearNumber && ratioLine) {
  ratioSlider.addEventListener("input", (event) => {
    const active = document.querySelector(".btn-circle.active");
    if (!active || String(active.dataset.mode || "").toLowerCase() !== "lock") {
      return;
    }
    const target = event.target;
    const rear = clampInt(target.value, 0, 50);
    const front = 100 - rear;

    lastLockRearBias = rear;
    target.value = String(rear);
    frontNumber.textContent = String(front);
    rearNumber.textContent = String(rear);
    updateRatioProgress(rear);
  });

  ratioSlider.addEventListener("change", async () => {
    const active = document.querySelector(".btn-circle.active");
    if (!active || String(active.dataset.mode || "").toLowerCase() !== "lock") {
      return;
    }
    try {
      await pushHomeMode(active);
    } catch {
      // Status poll will reconcile UI if write fails.
    }
  });
}

const savedTheme = getStoredTheme();
const initialTheme = savedTheme || document.body.dataset.theme || "dark";
applyTheme(initialTheme);
storeTheme(initialTheme);

updateThemeToggleLabel();
updateFullscreenLabel();

const activeModeButton = document.querySelector(".btn-circle.active") || buttons[0];
if (activeModeButton) {
  setMode(activeModeButton);
}

const pageName = String(document.body?.dataset?.page || "").toLowerCase();
initHomeTelemetry();
initCurvePage();
if (pageName === "map" && typeof initMapPage === "function") initMapPage();
if (pageName === "canview" && typeof initCanviewPage === "function") initCanviewPage();
if (pageName === "diag" && typeof initDiagPage === "function") initDiagPage();
if (pageName === "ota" && typeof initOtaPage === "function") initOtaPage();
initSetupPage();
initLogsPage();
initDetailsPersistence();
initCollapsibleCards();

// Merged from pages-legacy.js to keep ui-dev on a single app.js file.
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
  const mapDisengageToggle = document.getElementById("mapEnableDisengageSpeed");
  const btnSaveMapDisengage = document.getElementById("btnSaveMapDisengage");
  const mapThrottleGateInput = document.getElementById("mapDisableThrottle");
  const mapThrottleGateToggle = document.getElementById("mapEnableThrottleGate");
  const mapSpeedGateInput = document.getElementById("mapDisableSpeed");
  const mapSpeedGateToggle = document.getElementById("mapEnableSpeedGate");
  const mapReleaseRateInput = document.getElementById("mapReleaseRate");
  const mapReleaseRateToggle = document.getElementById("mapEnableReleaseRate");
  const mapBroadcastToggle = document.getElementById("mapBroadcastBridge");
  const mapControllerToggle = document.getElementById("mapControllerEnabled");

  const mapBehaviorCacheKeys = {
    disengage: "dynamic:disengage:map",
    throttleGate: "dynamic:global:disableThrottle",
    speedGate: "dynamic:global:disableSpeed",
    releaseRate: "dynamic:global:releaseRate",
  };

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

  function mapNameFromPath(path) {
    const value = String(path || "");
    const base = value.split("/").pop() || value;
    return base.replace(/\.(txt|json)$/i, "");
  }

  function mapLabel(entry) {
    const explicit = String(entry?.name || "").trim();
    if (explicit) return mapNameFromPath(explicit) || explicit;
    return mapNameFromPath(entry?.path || "") || "map";
  }

  function readToggleChecked(toggle, fallback = true) {
    return toggle ? Boolean(toggle.checked) : fallback;
  }

  function applyMapSettingFromStatus({
    apiValue,
    clampMax,
    input,
    toggle,
    cacheKey,
    fallbackValue = 0,
  }) {
    const fromApi = clamp(toInt(apiValue, 0), 0, clampMax);
    const cached = clamp(getModeBehaviorValue(cacheKey, fallbackValue), 0, clampMax);
    const enabled = fromApi > 0;
    const value = enabled ? fromApi : cached;
    if (input) input.value = String(value);
    if (toggle) toggle.checked = enabled;
    setModeBehaviorValue(cacheKey, value);
    return { enabled, value };
  }

  function getMapDisengageValue() {
    if (!mapDisengageInput) return 0;
    const value = clamp(toInt(mapDisengageInput.value, 0), 0, 300);
    mapDisengageInput.value = String(value);
    setModeBehaviorValue(mapBehaviorCacheKeys.disengage, value);
    return value;
  }

  function setMapDisengageValue(value) {
    if (!mapDisengageInput) return;
    const sanitized = clamp(toInt(value, 0), 0, 300);
    mapDisengageInput.value = String(sanitized);
    setModeBehaviorValue(mapBehaviorCacheKeys.disengage, sanitized);
  }

  function getMapThrottleGateValue() {
    if (!mapThrottleGateInput) return 0;
    const value = clamp(toInt(mapThrottleGateInput.value, 0), 0, 100);
    mapThrottleGateInput.value = String(value);
    setModeBehaviorValue(mapBehaviorCacheKeys.throttleGate, value);
    return value;
  }

  function setMapThrottleGateValue(value) {
    if (!mapThrottleGateInput) return;
    const sanitized = clamp(toInt(value, 0), 0, 100);
    mapThrottleGateInput.value = String(sanitized);
    setModeBehaviorValue(mapBehaviorCacheKeys.throttleGate, sanitized);
  }

  function getMapSpeedGateValue() {
    if (!mapSpeedGateInput) return 0;
    const value = clamp(toInt(mapSpeedGateInput.value, 0), 0, 300);
    mapSpeedGateInput.value = String(value);
    setModeBehaviorValue(mapBehaviorCacheKeys.speedGate, value);
    return value;
  }

  function setMapSpeedGateValue(value) {
    if (!mapSpeedGateInput) return;
    const sanitized = clamp(toInt(value, 0), 0, 300);
    mapSpeedGateInput.value = String(sanitized);
    setModeBehaviorValue(mapBehaviorCacheKeys.speedGate, sanitized);
  }

  function getMapReleaseRateValue() {
    if (!mapReleaseRateInput) return 120;
    const value = clamp(toInt(mapReleaseRateInput.value, 120), 0, 1000);
    mapReleaseRateInput.value = String(value);
    setModeBehaviorValue(mapBehaviorCacheKeys.releaseRate, value);
    return value;
  }

  function setMapReleaseRateValue(value) {
    if (!mapReleaseRateInput) return;
    const sanitized = clamp(toInt(value, 120), 0, 1000);
    mapReleaseRateInput.value = String(sanitized);
    setModeBehaviorValue(mapBehaviorCacheKeys.releaseRate, sanitized);
  }
  function summarizeMapModeSettings(settings) {
    const disengage =
      settings.disengageEnabled && settings.disengage > 0
        ? `Disengage below ${settings.disengage} km/h.`
        : settings.disengage > 0
          ? `Disengage gate off (saved ${settings.disengage} km/h).`
          : "Disengage gate off.";
    const throttleGate =
      settings.disableThrottleEnabled && settings.disableThrottle > 0
        ? `Throttle >= ${settings.disableThrottle}%.`
        : settings.disableThrottle > 0
          ? `Throttle gate off (saved ${settings.disableThrottle}%).`
          : "Throttle gate off.";
    const speedGate =
      settings.disableSpeedEnabled && settings.disableSpeed > 0
        ? `Disable above ${settings.disableSpeed} km/h.`
        : settings.disableSpeed > 0
          ? `High-speed gate off (saved ${settings.disableSpeed} km/h).`
          : "High-speed gate off.";
    const releaseRate =
      settings.releaseRateEnabled && settings.releaseRate > 0
        ? `Release ramp ${settings.releaseRate} %/s.`
        : settings.releaseRate > 0
          ? `Release ramp off (saved ${settings.releaseRate} %/s).`
          : "Release ramp off.";
    const broadcast = settings.broadcastOpenHaldexOverCAN ? "Bridge on." : "Bridge off.";
    const controller = settings.disableController ? "Controller off." : "Controller on.";
    return `${disengage} ${throttleGate} ${speedGate} ${releaseRate} ${broadcast} ${controller}`;
  }

  function applyMapModeSettingsFromStatus(data) {
    const disengage = applyMapSettingFromStatus({
      apiValue: data && data.disengageUnderSpeed ? data.disengageUnderSpeed.map : 0,
      clampMax: 300,
      input: mapDisengageInput,
      toggle: mapDisengageToggle,
      cacheKey: mapBehaviorCacheKeys.disengage,
      fallbackValue: 0,
    });
    const throttleGate = applyMapSettingFromStatus({
      apiValue: data ? data.disableThrottle : 0,
      clampMax: 100,
      input: mapThrottleGateInput,
      toggle: mapThrottleGateToggle,
      cacheKey: mapBehaviorCacheKeys.throttleGate,
      fallbackValue: 0,
    });
    const speedGate = applyMapSettingFromStatus({
      apiValue: data ? data.disableSpeed : 0,
      clampMax: 300,
      input: mapSpeedGateInput,
      toggle: mapSpeedGateToggle,
      cacheKey: mapBehaviorCacheKeys.speedGate,
      fallbackValue: 0,
    });
    const releaseRate = applyMapSettingFromStatus({
      apiValue: data ? (data.lockReleaseRatePctPerSec ?? 120) : 120,
      clampMax: 1000,
      input: mapReleaseRateInput,
      toggle: mapReleaseRateToggle,
      cacheKey: mapBehaviorCacheKeys.releaseRate,
      fallbackValue: 120,
    });
    if (mapBroadcastToggle)
      mapBroadcastToggle.checked = !data || data.broadcastOpenHaldexOverCAN !== false;
    if (mapControllerToggle)
      mapControllerToggle.checked = !data || !Boolean(data.disableController);
    return {
      disengage: disengage.value,
      disengageEnabled: disengage.enabled,
      disableThrottle: throttleGate.value,
      disableThrottleEnabled: throttleGate.enabled,
      disableSpeed: speedGate.value,
      disableSpeedEnabled: speedGate.enabled,
      releaseRate: releaseRate.value,
      releaseRateEnabled: releaseRate.enabled,
      broadcastOpenHaldexOverCAN: mapBroadcastToggle ? Boolean(mapBroadcastToggle.checked) : true,
      disableController: mapControllerToggle ? !Boolean(mapControllerToggle.checked) : false,
    };
  }

  function readMapModeSettingsPayload() {
    const disengage = getMapDisengageValue();
    const disableThrottle = getMapThrottleGateValue();
    const disableSpeed = getMapSpeedGateValue();
    const releaseRate = getMapReleaseRateValue();
    const settings = {
      disengage,
      disengageEnabled: readToggleChecked(mapDisengageToggle, disengage > 0),
      disableThrottle,
      disableThrottleEnabled: readToggleChecked(mapThrottleGateToggle, disableThrottle > 0),
      disableSpeed,
      disableSpeedEnabled: readToggleChecked(mapSpeedGateToggle, disableSpeed > 0),
      releaseRate,
      releaseRateEnabled: readToggleChecked(mapReleaseRateToggle, releaseRate > 0),
      broadcastOpenHaldexOverCAN: mapBroadcastToggle ? Boolean(mapBroadcastToggle.checked) : true,
      disableController: mapControllerToggle ? !Boolean(mapControllerToggle.checked) : false,
    };
    return {
      settings,
      payload: {
        disableThrottle: settings.disableThrottleEnabled ? settings.disableThrottle : 0,
        disableSpeed: settings.disableSpeedEnabled ? settings.disableSpeed : 0,
        broadcastOpenHaldexOverCAN: settings.broadcastOpenHaldexOverCAN,
        disableController: settings.disableController,
        lockReleaseRatePctPerSec: settings.releaseRateEnabled ? settings.releaseRate : 0,
        disengageUnderSpeed: {
          map: settings.disengageEnabled ? settings.disengage : 0,
        },
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
    const isLight =
      document.body && document.body.dataset && document.body.dataset.theme === "light";
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
    const { payload, settings: localSettings } = readMapModeSettingsPayload();
    await fetchJson("/api/settings", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
    let settings = {
      disengage: localSettings.disengage,
      disengageEnabled: localSettings.disengageEnabled,
      disableThrottle: localSettings.disableThrottle,
      disableThrottleEnabled: localSettings.disableThrottleEnabled,
      disableSpeed: localSettings.disableSpeed,
      disableSpeedEnabled: localSettings.disableSpeedEnabled,
      broadcastOpenHaldexOverCAN: localSettings.broadcastOpenHaldexOverCAN,
      disableController: localSettings.disableController,
      releaseRate: localSettings.releaseRate,
      releaseRateEnabled: localSettings.releaseRateEnabled,
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
        opt.textContent = mapLabel(entry);
        mapSelect.appendChild(opt);
      });
      if (current && !Array.from(mapSelect.options).some((o) => o.value === current)) {
        const opt = document.createElement("option");
        const name = mapNameFromPath(current) || "current";
        opt.value = current;
        opt.textContent = name;
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
  if (mapReleaseRateInput) {
    mapReleaseRateInput.onchange = () => {
      setMapReleaseRateValue(mapReleaseRateInput.value);
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
        wifiApStatus.textContent = data.apPasswordSet
          ? "AP password is set"
          : "AP is open (no password)";
      }
      if (wifiStatus) {
        wifiStatus.textContent = data.ssid
          ? `Saved SSID: ${data.ssid} | ${apMode}`
          : `No saved hotspot | ${apMode}`;
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
