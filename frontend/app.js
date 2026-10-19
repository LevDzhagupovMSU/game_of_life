const elements = {
  board: document.querySelector("#board"),
  stage: document.querySelector("#board-stage"),
  boardSize: document.querySelector("#board-size-label"),
  boardHint: document.querySelector("#board-hint"),
  width: document.querySelector("#width-range"),
  height: document.querySelector("#height-range"),
  widthValue: document.querySelector("#width-value"),
  heightValue: document.querySelector("#height-value"),
  tick: document.querySelector("#tick-range"),
  tickValue: document.querySelector("#tick-value"),
  sizeHelp: document.querySelector("#size-help"),
  url: document.querySelector("#server-url"),
  status: document.querySelector("#connection-status"),
  statusText: document.querySelector("#status-text"),
  generation: document.querySelector("#generation-value"),
  alive: document.querySelector("#alive-value"),
  density: document.querySelector("#density-value"),
  play: document.querySelector("#play-button"),
  playLabel: document.querySelector("#play-label"),
  step: document.querySelector("#step-button"),
  reset: document.querySelector("#reset-button"),
};

const context = elements.board.getContext("2d");
const state = {
  width: Number(elements.width.value),
  height: Number(elements.height.value),
  cells: new Uint8Array(Number(elements.width.value) * Number(elements.height.value)),
  generation: 0,
  sizeLocked: false,
  editable: true,
  running: false,
  connecting: false,
  pending: false,
  initialized: false,
  socket: null,
  connectionToken: 0,
  timer: null,
  paintValue: null,
  hoverIndex: -1,
  cellSize: 20,
};

elements.url.value = `ws://${window.location.hostname || "localhost"}:8080/`;

function setStatus(message, tone = "idle") {
  elements.statusText.textContent = message;
  elements.status.dataset.tone = tone;
}

function updateControls() {
  elements.width.disabled = state.sizeLocked || state.connecting;
  elements.height.disabled = state.sizeLocked || state.connecting;
  elements.url.disabled = Boolean(state.socket);
  elements.step.disabled = state.pending || state.connecting;
  elements.playLabel.textContent = state.running
    ? "Пауза"
    : state.initialized ? "Продолжить" : "Запустить";
  elements.play.querySelector(".button-icon").textContent = state.running ? "Ⅱ" : "▶";
  elements.board.classList.toggle("is-locked", !state.editable);
  elements.boardHint.textContent = state.editable
    ? "Нажимайте и проводите по клеткам, чтобы нарисовать начальное состояние."
    : "Поле показывает последнее состояние сервера. Для нового рисунка нажмите «Новая партия».";
  elements.sizeHelp.textContent = state.sizeLocked
    ? "Размер закреплён после первого запуска. Для другого размера перезагрузите страницу."
    : "Размер можно изменить до первого запуска. После этого он закрепляется.";
}

function updateStats() {
  let alive = 0;
  for (const cell of state.cells) alive += cell;
  elements.generation.textContent = String(state.generation).padStart(2, "0");
  elements.alive.textContent = String(alive);
  elements.density.textContent = `${Math.round(alive / state.cells.length * 100)}%`;
  elements.boardSize.textContent = `${state.width} × ${state.height}`;
  elements.widthValue.textContent = String(state.width);
  elements.heightValue.textContent = String(state.height);
}

function resizeCanvas() {
  const availableWidth = Math.max(100, elements.stage.clientWidth - 36);
  state.cellSize = Math.min(30, availableWidth / state.width, 620 / state.height);
  const width = state.width * state.cellSize;
  const height = state.height * state.cellSize;
  const ratio = window.devicePixelRatio || 1;
  elements.board.style.width = `${width}px`;
  elements.board.style.height = `${height}px`;
  elements.board.width = Math.round(width * ratio);
  elements.board.height = Math.round(height * ratio);
  context.setTransform(elements.board.width / width, 0, 0, elements.board.height / height, 0, 0);
  drawBoard();
}

function drawBoard() {
  const size = state.cellSize;
  const width = state.width * size;
  const height = state.height * size;
  context.fillStyle = "#101e1d";
  context.fillRect(0, 0, width, height);

  context.strokeStyle = "#33453f";
  context.lineWidth = 1;
  context.beginPath();
  for (let column = 1; column < state.width; column++) {
    const x = Math.round(column * size) + 0.5;
    context.moveTo(x, 0);
    context.lineTo(x, height);
  }
  for (let row = 1; row < state.height; row++) {
    const y = Math.round(row * size) + 0.5;
    context.moveTo(0, y);
    context.lineTo(width, y);
  }
  context.stroke();

  const inset = Math.max(1, size * 0.075);
  context.fillStyle = "#b7e56e";
  for (let index = 0; index < state.cells.length; index++) {
    if (!state.cells[index]) continue;
    const x = (index % state.width) * size;
    const y = Math.floor(index / state.width) * size;
    context.fillRect(x + inset, y + inset, size - inset * 2, size - inset * 2);
  }

  if (state.editable && state.hoverIndex >= 0) {
    const x = (state.hoverIndex % state.width) * size;
    const y = Math.floor(state.hoverIndex / state.width) * size;
    context.strokeStyle = "#f2ffc3";
    context.lineWidth = 2;
    context.strokeRect(x + 2, y + 2, size - 4, size - 4);
  }
}

function cellAt(event) {
  const rect = elements.board.getBoundingClientRect();
  const column = Math.floor((event.clientX - rect.left) / rect.width * state.width);
  const row = Math.floor((event.clientY - rect.top) / rect.height * state.height);
  if (column < 0 || row < 0 || column >= state.width || row >= state.height) return -1;
  return row * state.width + column;
}

function paint(index) {
  if (index < 0 || state.cells[index] === state.paintValue) return;
  state.cells[index] = state.paintValue;
  drawBoard();
  updateStats();
}

elements.board.addEventListener("pointerdown", event => {
  if (!state.editable || state.connecting || state.pending || (event.pointerType === "mouse" && event.button !== 0)) return;
  const index = cellAt(event);
  if (index < 0) return;
  state.paintValue = state.cells[index] ? 0 : 1;
  state.hoverIndex = index;
  elements.board.setPointerCapture(event.pointerId);
  paint(index);
  event.preventDefault();
});

elements.board.addEventListener("pointermove", event => {
  if (!state.editable) return;
  const index = cellAt(event);
  if (index === state.hoverIndex && state.paintValue === null) return;
  state.hoverIndex = index;
  if (state.paintValue === null) drawBoard();
  else paint(index);
});

function finishPainting() { state.paintValue = null; }
elements.board.addEventListener("pointerup", finishPainting);
elements.board.addEventListener("pointercancel", finishPainting);
elements.board.addEventListener("pointerleave", () => {
  if (state.paintValue === null) {
    state.hoverIndex = -1;
    drawBoard();
  }
});

function stopTimer() {
  if (state.timer !== null) window.clearInterval(state.timer);
  state.timer = null;
}

function pause() {
  state.running = false;
  stopTimer();
  updateControls();
}

function disconnect() {
  state.connectionToken++;
  const socket = state.socket;
  state.socket = null;
  state.connecting = false;
  state.pending = false;
  state.initialized = false;
  if (socket && socket.readyState !== WebSocket.CLOSED) socket.close();
  updateControls();
}

function startTimer() {
  if (!state.running || !state.initialized || state.timer !== null) return;
  state.timer = window.setInterval(() => {
    if (!state.pending) sendStep();
  }, Number(elements.tick.value));
}

function sendStep() {
  const socket = state.socket;
  if (!socket || socket.readyState !== WebSocket.OPEN || state.pending) return;
  const isFirstStep = !state.initialized;
  const payload = isFirstStep
    ? JSON.stringify({ width: state.width, height: state.height, grid: Array.from(state.cells) })
    : "{}";
  try {
    socket.send(payload);
  } catch (error) {
    pause();
    disconnect();
    state.editable = true;
    setStatus(`Не удалось отправить запрос: ${error.message}`, "error");
    updateControls();
    return;
  }
  state.pending = true;
  if (isFirstStep) {
    state.sizeLocked = true;
    state.editable = false;
    state.hoverIndex = -1;
    drawBoard();
  }
  setStatus("Вычисляется шаг…", "ready");
  updateControls();
}

function handleResponse(data) {
  let response;
  try {
    response = JSON.parse(data);
  } catch {
    response = { error: "Сервер вернул некорректный JSON" };
  }
  state.pending = false;

  if (typeof response.error === "string") {
    pause();
    disconnect();
    state.editable = true;
    setStatus(response.error, "error");
    updateControls();
    return;
  }
  if (!Array.isArray(response.state) || response.state.length !== state.cells.length ||
      !response.state.every(cell => cell === 0 || cell === 1)) {
    pause();
    disconnect();
    state.editable = true;
    setStatus("Сервер вернул поле неверного размера или формата", "error");
    updateControls();
    return;
  }

  state.cells = Uint8Array.from(response.state);
  state.generation++;
  state.initialized = true;
  state.editable = false;
  drawBoard();
  updateStats();
  setStatus(state.running ? "Симуляция запущена" : "Пауза · соединение активно", "ready");
  startTimer();
  updateControls();
}

function connectAndSend() {
  const address = elements.url.value.trim();
  try {
    const parsed = new URL(address);
    if (parsed.protocol !== "ws:" && parsed.protocol !== "wss:") throw new Error("Нужен адрес ws:// или wss://");
  } catch (error) {
    pause();
    setStatus(`Неверный адрес сервера: ${error.message}`, "error");
    return;
  }

  let socket;
  try { socket = new WebSocket(address); }
  catch (error) {
    pause();
    setStatus(`Не удалось подключиться: ${error.message}`, "error");
    return;
  }
  const token = ++state.connectionToken;
  state.socket = socket;
  state.connecting = true;
  setStatus("Подключение к серверу…");
  updateControls();

  socket.addEventListener("open", () => {
    if (token !== state.connectionToken) return;
    state.connecting = false;
    sendStep();
  });
  socket.addEventListener("message", event => {
    if (token === state.connectionToken) handleResponse(event.data);
  });
  socket.addEventListener("close", () => {
    if (token !== state.connectionToken) return;
    pause();
    disconnect();
    state.editable = true;
    setStatus("Соединение закрыто. Можно запустить снова с текущего поля.", "error");
    updateControls();
  });
  socket.addEventListener("error", () => {
    if (token === state.connectionToken) setStatus("Ошибка WebSocket-соединения", "error");
  });
}

function runOrPause() {
  if (state.running) {
    pause();
    if (state.connecting) {
      disconnect();
      setStatus("Подключение отменено");
    } else {
      setStatus(state.pending ? "Ожидаем ответ на последний шаг…" : "Пауза · соединение активно", "ready");
    }
    return;
  }
  state.running = true;
  updateControls();
  if (state.socket?.readyState === WebSocket.OPEN) {
    if (state.initialized) {
      setStatus("Симуляция запущена", "ready");
      startTimer();
    } else sendStep();
  } else connectAndSend();
}

elements.play.addEventListener("click", runOrPause);
elements.step.addEventListener("click", () => {
  pause();
  if (state.socket?.readyState === WebSocket.OPEN) sendStep();
  else connectAndSend();
});
elements.reset.addEventListener("click", () => {
  pause();
  disconnect();
  state.cells = new Uint8Array(state.width * state.height);
  state.generation = 0;
  state.editable = true;
  state.hoverIndex = -1;
  drawBoard();
  updateStats();
  updateControls();
  setStatus("Новая партия готова");
});

function changeSize() {
  if (state.sizeLocked || state.connecting) return;
  state.width = Number(elements.width.value);
  state.height = Number(elements.height.value);
  state.cells = new Uint8Array(state.width * state.height);
  state.generation = 0;
  state.hoverIndex = -1;
  resizeCanvas();
  updateStats();
}
elements.width.addEventListener("input", changeSize);
elements.height.addEventListener("input", changeSize);
elements.tick.addEventListener("input", () => {
  const milliseconds = Number(elements.tick.value);
  elements.tickValue.textContent = milliseconds < 1000
    ? `${milliseconds} мс`
    : `${(milliseconds / 1000).toLocaleString("ru-RU", { maximumFractionDigits: 2 })} с`;
  if (state.timer !== null) {
    stopTimer();
    startTimer();
  }
});

if (window.ResizeObserver) new ResizeObserver(resizeCanvas).observe(elements.stage);
else window.addEventListener("resize", resizeCanvas);
resizeCanvas();
updateStats();
updateControls();
