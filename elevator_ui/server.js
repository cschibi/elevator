'use strict';

const { spawn } = require('child_process');
const http      = require('http');
const fs        = require('fs');
const path      = require('path');
const WebSocket = require('ws');

const SIM_BIN = path.resolve(__dirname, '..', 'elevator_sim');
const UI_HTML = path.join(__dirname, 'elevator_ui.html');
const PORT    = 8080;

// ── Simulator process & I/O state ────────────────────────
let sim      = null;
let rawBuf   = '';
let simReady = false;
let cmdQueue = [];

// ── Randomization state ───────────────────────────────────
let randActive     = false;
let randTimer      = null;
let lastQueueDepth = 0;
let lastNumFloors  = 90;

// Parsing context — updated as stdout lines arrive
const ctx = {
  numFloors:    null,
  numElevators: null,
  scheduler:    null,
  inTick:       false,
  tick:         0,
  elevators:    [],
  queueDepth:   0,
  inMetrics:    false,
  metrics:      null,
};

// ── WebSocket clients ─────────────────────────────────────
const clients = new Set();

function broadcast(obj) {
  const txt = JSON.stringify(obj);
  clients.forEach(ws => {
    if (ws.readyState === WebSocket.OPEN) ws.send(txt);
  });
}

// ── ANSI escape stripping ─────────────────────────────────
function stripAnsi(s) {
  // eslint-disable-next-line no-control-regex
  return s.replace(/\x1b\[[0-9;]*m/g, '');
}

// ── Line parser ───────────────────────────────────────────
// Called for each complete line from simulator stdout.
function parseLine(raw) {
  const line = stripAnsi(raw);
  if (!line.trim()) return;

  // Log line: [T0001][INF][file.c:30] message
  {
    const m = line.match(/^\[T(\d+)\]\[(\w+)\]\[[^\]]+\]\s*(.*)/);
    if (m) {
      const message = m[3];
      // Detect runtime scheduler swap from the log message itself
      const sm = message.match(/Scheduler switched to (\S+)/i);
      if (sm) ctx.scheduler = sm[1].toUpperCase();
      broadcast({ type: 'log', tick: +m[1], level: m[2], message });
      return;
    }
  }

  // Config block printed at startup
  {
    let m;
    if ((m = line.match(/^\s+Floors\s*:\s*(\d+)/)))     { ctx.numFloors    = +m[1]; return; }
    if ((m = line.match(/^\s+Elevators\s*:\s*(\d+)/)))  { ctx.numElevators = +m[1]; return; }
    if ((m = line.match(/^\s+Scheduler\s*:\s*(\S+)/)))  { ctx.scheduler    = m[1];  return; }
  }

  // Tick status header: "── Tick 10 ──────────────────────"
  // The ─ character is U+2500; the line never starts with '['.
  if (!line.startsWith('[')) {
    const m = line.match(/\bTick\s+(\d+)\b/);
    if (m) {
      ctx.inTick    = true;
      ctx.tick      = +m[1];
      ctx.elevators = [];
      return;
    }
  }

  // Lines inside a tick block
  if (ctx.inTick) {
    // Elevator line:
    // "  [E0] Floor:3 Dir:UP    State:DOOR_OPEN  Door:OPEN     Stops:[4 ] dist:3 pax:1"
    {
      const m = line.match(
        /\[E(\d+)\]\s+Floor:(\d+)\s+Dir:(\S+)\s+State:(\S+)\s+Door:(\S+)\s+Stops:\[([^\]]*)\]\s+dist:(\d+)\s+pax:(\d+)(?:\s+idle:(\d+))?/
      );
      if (m) {
        const stopsStr = m[6].trim();
        ctx.elevators.push({
          id:        +m[1],
          floor:     +m[2],
          direction: m[3].trim(),
          state:     m[4].trim(),
          door:      m[5].trim(),
          stops:     stopsStr ? stopsStr.split(/\s+/).map(Number).filter(n => !isNaN(n)) : [],
          dist:      +m[7],
          pax:       +m[8],
          idleTicks: m[9] !== undefined ? +m[9] : 0,
        });
        return;
      }
    }
    // Queue depth line (marks end of tick block)
    {
      const m = line.match(/Queue depth:\s*(\d+)/);
      if (m) {
        ctx.queueDepth = +m[1];
        lastQueueDepth = ctx.queueDepth;
        if (ctx.numFloors) lastNumFloors = ctx.numFloors;
        broadcast({
          type:         'state',
          tick:         ctx.tick,
          numFloors:    ctx.numFloors,
          numElevators: ctx.numElevators,
          scheduler:    ctx.scheduler,
          elevators:    [...ctx.elevators],
          queueDepth:   ctx.queueDepth,
        });
        ctx.inTick = false;
        return;
      }
    }
  }

  // Metrics block
  if (line.includes('=== Simulation Metrics ===')) {
    ctx.inMetrics = true;
    ctx.metrics   = { elevators: [] };
    return;
  }
  if (ctx.inMetrics) {
    let m;
    if ((m = line.match(/Ticks elapsed\s*:\s*(\d+)/)))         { ctx.metrics.ticks        = +m[1]; return; }
    if ((m = line.match(/Total requests\s*:\s*(\d+)/)))        { ctx.metrics.totalRequests = +m[1]; return; }
    if ((m = line.match(/Total served\s*:\s*(\d+)/)))          { ctx.metrics.totalServed   = +m[1]; return; }
    if ((m = line.match(/Scheduler\s*:\s*(\S+)/)))             { ctx.metrics.scheduler     = m[1];  return; }
    if ((m = line.match(/Elevator (\d+)\s*:\s*dist=(\d+)\s+pax=(\d+)/))) {
      ctx.metrics.elevators.push({ id: +m[1], dist: +m[2], pax: +m[3] });
      return;
    }
    if (line.includes('==========================')) {
      broadcast({ type: 'metrics', ...ctx.metrics });
      ctx.inMetrics = false;
      return;
    }
  }
}

// ── stdout buffer processor ───────────────────────────────
// Logger flushes each log line immediately; the status block and
// prompt are flushed together by the CLI loop's fflush(stdout).
// We process complete lines and detect the prompt (no trailing \n).
function processBuffer() {
  let nlIdx;
  while ((nlIdx = rawBuf.indexOf('\n')) !== -1) {
    const line = rawBuf.slice(0, nlIdx);
    rawBuf     = rawBuf.slice(nlIdx + 1);
    parseLine(line);
  }
  // Prompt pattern: "[T0042]> " with no trailing newline
  if (/^\[T\d{4}\]>\s*$/.test(rawBuf)) {
    rawBuf   = '';
    simReady = true;
    processNext();
  }
}

// ── Command queue ─────────────────────────────────────────
function enqueue(cmd) {
  cmdQueue.push(cmd);
  if (simReady) processNext();
}

function processNext() {
  if (!simReady || cmdQueue.length === 0) return;
  const cmd = cmdQueue.shift();
  simReady  = false;
  sim.stdin.write(cmd + '\n');
}

// ── Request randomization ─────────────────────────────────
function doRandomRequest() {
  if (lastQueueDepth >= 100) {
    stopRandomization();
    broadcast({ type: 'randomStatus', active: false, reason: 'queue_full' });
    return;
  }
  const floor = Math.floor(Math.random() * lastNumFloors);
  let dir;
  if      (floor === 0)                  dir = 'u';
  else if (floor === lastNumFloors - 1)  dir = 'd';
  else                                   dir = Math.random() < 0.5 ? 'u' : 'd';
  enqueue(`r ${floor} ${dir}`);
  enqueue('s');
  enqueue('p');
}

function startRandomization() {
  if (randActive) return;
  randActive = true;
  broadcast({ type: 'randomStatus', active: true });
  randTimer = setInterval(doRandomRequest, 3000);
}

function stopRandomization() {
  if (!randTimer) return;
  clearInterval(randTimer);
  randTimer  = null;
  randActive = false;
  broadcast({ type: 'randomStatus', active: false });
}

// ── Spawn simulator ───────────────────────────────────────
function startSim() {
  if (!fs.existsSync(SIM_BIN)) {
    console.error(`[server] ERROR: simulator binary not found at:\n         ${SIM_BIN}`);
    console.error(`[server] Run 'make' in the project root first.`);
    process.exit(1);
  }

  sim = spawn(SIM_BIN, []);

  sim.stdout.on('data', chunk => {
    rawBuf += chunk.toString('utf8');
    processBuffer();
  });

  sim.stderr.on('data', chunk => {
    console.error('[sim stderr]', chunk.toString().trimEnd());
  });

  sim.on('close', code => {
    console.log(`[server] simulator exited (code ${code})`);
    broadcast({ type: 'exit', code });
  });

  console.log(`[server] simulator started  pid=${sim.pid}`);
}

// ── HTTP server (serves the UI file) ─────────────────────
const httpServer = http.createServer((req, res) => {
  if (req.url === '/' || req.url === '/elevator_ui.html') {
    fs.readFile(UI_HTML, (err, data) => {
      if (err) { res.writeHead(404); res.end('Not found'); return; }
      res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
      res.end(data);
    });
    return;
  }
  res.writeHead(404);
  res.end('Not found');
});

// ── WebSocket server ──────────────────────────────────────
const wss = new WebSocket.Server({ server: httpServer });

wss.on('connection', ws => {
  clients.add(ws);
  console.log('[server] client connected');

  // Send current building state to the new client immediately
  enqueue('p');

  ws.on('message', raw => {
    let msg;
    try { msg = JSON.parse(raw.toString()); } catch { return; }

    switch (msg.type) {
      case 'step':
        enqueue('s');
        break;
      case 'auto':
        enqueue(`a ${Math.max(1, +msg.n || 1)}`);
        break;
      case 'request':
        enqueue(`r ${msg.floor} ${msg.direction === 'UP' ? 'u' : 'd'}`);
        enqueue('p');   // refresh state after queuing request
        break;
      case 'scheduler':
        enqueue(`S ${msg.name}`);
        enqueue('p');   // refresh state (and pick up updated scheduler name)
        break;
      case 'status':
        enqueue('p');
        break;
      case 'startRandom':
        startRandomization();
        break;
      case 'stopRandom':
        stopRandomization();
        break;
    }
  });

  ws.on('close', () => {
    clients.delete(ws);
    console.log('[server] client disconnected');
  });
});

// ── Start ─────────────────────────────────────────────────
httpServer.listen(PORT, () => {
  console.log(`[server] http://localhost:${PORT}`);
  startSim();
});

process.on('SIGINT', () => {
  console.log('\n[server] shutting down…');
  if (sim) sim.stdin.write('q\n');
  setTimeout(() => process.exit(0), 500);
});
