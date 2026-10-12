import http from 'node:http';
import { execFile } from 'node:child_process';
import { randomUUID } from 'node:crypto';
import { mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { promisify } from 'node:util';

const exec = promisify(execFile);
const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, '..');
const publicDir = path.join(here, 'public');
const buildDir = process.env.STRELA_BUILD_DIR || path.join(root, 'build');
const suffix = process.platform === 'win32' ? '.exe' : '';
const runner = path.join(buildDir, 'strela_run' + suffix);
const viewer = path.join(buildDir, 'strela_view' + suffix);
const host = '127.0.0.1';
const port = Number(process.env.STRELA_PORT || 4317);
const sessions = new Map();
const tempRoot = await mkdtemp(path.join(os.tmpdir(), 'strela-web-'));

function fail(status, message) {
  const error = new Error(message);
  error.status = status;
  throw error;
}

async function binary(file, args, accepted = [0]) {
  try {
    return await exec(file, args, { timeout: 15000, maxBuffer: 2 * 1024 * 1024, windowsHide: true });
  } catch (error) {
    if (accepted.includes(Number(error.code))) return error;
    fail(422, (error.stderr || error.message || 'Emulator command failed').trim());
  }
}

async function body(req, limit = 16 * 1024 * 1024) {
  let size = 0;
  const parts = [];
  for await (const chunk of req) {
    size += chunk.length;
    if (size > limit) fail(413, 'Upload is too large');
    parts.push(chunk);
  }
  return Buffer.concat(parts);
}

async function jsonBody(req, limit = 1024 * 1024) {
  const bytes = await body(req, limit);
  try { return JSON.parse(bytes.toString('utf8')); }
  catch { fail(400, 'Invalid JSON request'); }
}

function send(res, status, value) {
  const payload = Buffer.from(JSON.stringify(value));
  res.writeHead(status, { 'Content-Type': 'application/json; charset=utf-8', 'Content-Length': payload.length, 'Cache-Control': 'no-store' });
  res.end(payload);
}

function oct4(value, max = 0o3777) {
  return typeof value === 'string' && /^[0-7]{4}$/.test(value) && parseInt(value, 8) <= max;
}

async function view(session, url) {
  if (url.searchParams.has('cards')) {
    const start = Number(url.searchParams.get('cards'));
    const count = Number(url.searchParams.get('count') || '128');
    if (!Number.isSafeInteger(start) || start < 0 || start > 0xffffffff ||
        !Number.isInteger(count) || count < 1 || count > 128)
      fail(400, 'Invalid punched deck range');
    const result = await binary(viewer, [session.snapshot, '--cards', String(start), '--count', String(count)]);
    return JSON.parse(result.stdout);
  }
  const ram = url.searchParams.get('ram') || '0000';
  const zone = url.searchParams.get('zone');
  if (!oct4(ram) || (zone !== null &&
      !(oct4(zone, 0o5777) && /^[45][0-7]{3}$/.test(zone) && !/^[45]000$/.test(zone))))
    fail(400, 'Invalid octal memory or tape address');
  const args = [session.snapshot, '--ram', ram];
  if (zone) args.push('--zone', zone);
  const result = await binary(viewer, args);
  return JSON.parse(result.stdout);
}

function enqueue(session, operation) {
  const pending = session.queue.then(operation, operation);
  session.queue = pending.catch(() => {});
  return pending;
}

async function createFromListing(req) {
  const payload = await jsonBody(req, 16 * 1024 * 1024);
  if (typeof payload.listing !== 'string' || !payload.listing.trim() ||
      Buffer.byteLength(payload.listing) > 1024 * 1024)
    fail(400, 'Provide an octal listing up to 1 MB');
  if (payload.start !== undefined && !oct4(payload.start))
    fail(400, 'Start address must be four octal digits');
  for (const key of ['cards', 'tape1', 'tape2']) {
    if (payload[key] !== undefined &&
        (typeof payload[key] !== 'string' || Buffer.byteLength(payload[key]) > 8 * 1024 * 1024))
      fail(400, key + ' image must be text under 8 MB');
  }
  const id = randomUUID();
  const listing = path.join(tempRoot, id + '.strela');
  const snapshot = path.join(tempRoot, id + '.sts');
  const inputs = [listing];
  const args = ['prepare', listing, '--save-snapshot', snapshot];
  if (payload.start) args.push('--start', payload.start);
  try {
    await writeFile(listing, payload.listing, 'utf8');
    for (const key of ['cards', 'tape1', 'tape2']) {
      if (payload[key] !== undefined) {
        const file = path.join(tempRoot, id + '.' + key);
        inputs.push(file);
        await writeFile(file, payload[key], 'utf8');
        args.push('--' + key, file);
      }
    }
    await binary(runner, args);
    const session = { snapshot, queue: Promise.resolve() };
    const state = await view(session, new URL('http://localhost/?ram=0000'));
    sessions.set(id, session);
    return { id, state };
  } catch (error) {
    await rm(snapshot, { force: true });
    throw error;
  } finally {
    for (const file of inputs) await rm(file, { force: true });
  }
}
async function importSnapshot(req) {
  const bytes = await body(req);
  if (!bytes.length) fail(400, 'Snapshot file is empty');
  const id = randomUUID();
  const snapshot = path.join(tempRoot, id + '.sts');
  await writeFile(snapshot, bytes);
  const session = { snapshot, queue: Promise.resolve() };
  try {
    const state = await view(session, new URL('http://localhost/?ram=0000'));
    sessions.set(id, session);
    return { id, state };
  } catch (error) {
    await rm(snapshot, { force: true });
    throw error;
  }
}

async function act(session, input) {
  if (!input || typeof input !== 'object') fail(400, 'Invalid action');
  const next = path.join(tempRoot, randomUUID() + '.sts');
  let args;
  switch (input.type) {
  case 'step':
    if (!Number.isInteger(input.count) || input.count < 1 || input.count > 10000)
      fail(400, 'Step count must be 1 to 10000');
    args = ['operate', session.snapshot, '--step', String(input.count)];
    break;
  case 'run':
    if (!Number.isSafeInteger(input.budgetUs) || input.budgetUs < 1 || input.budgetUs > 1000000000)
      fail(400, 'Time budget must be 1 to 1000000000 microseconds');
    args = ['resume', session.snapshot, '--max-us', String(input.budgetUs)];
    break;
  case 'reset':
    if (!oct4(input.address)) fail(400, 'Reset address must be four octal digits');
    args = ['operate', session.snapshot, '--reset', input.address];
    break;
  case 'set':
    if (!oct4(input.address, 0o7777) || typeof input.word !== 'string' ||
        !/^[0-7]{15}$/.test(input.word) || parseInt(input.word, 8) > 0x7ffffffffff)
      fail(400, 'Provide a four-digit octal address and a valid 43-bit word');
    args = ['operate', session.snapshot, '--set', input.address, input.word];
    break;
  case 'check':
    if (typeof input.enabled !== 'boolean') fail(400, 'Invalid check switch value');
    args = ['operate', session.snapshot, '--check-stops', input.enabled ? '1' : '0'];
    break;
  default: fail(400, 'Unknown operator action');
  }
  args.push('--save-snapshot', next);
  try {
    await binary(runner, args, [0, 1, 3]);
    const previous = session.snapshot;
    session.snapshot = next;
    await rm(previous, { force: true });
  } catch (error) {
    await rm(next, { force: true });
    throw error;
  }
}

const assets = new Map([
  ['/', ['index.html', 'text/html; charset=utf-8']],
  ['/index.html', ['index.html', 'text/html; charset=utf-8']],
  ['/style.css', ['style.css', 'text/css; charset=utf-8']],
  ['/strela-control-panel-polymus.jpg', ['strela-control-panel-polymus.jpg', 'image/jpeg']],
  ['/strela-lamp-panel-polymus.jpg', ['strela-lamp-panel-polymus.jpg', 'image/jpeg']],
  ['/app.js', ['app.js', 'text/javascript; charset=utf-8']],
  ['/vostok-orbit.strela', ['../vostok-orbit.strela', 'text/plain; charset=utf-8']]
]);

const server = http.createServer(async (req, res) => {
  try {
    const url = new URL(req.url, 'http://localhost');
    if (req.method === 'GET' && url.pathname === '/api/example') {
      const listing = await readFile(path.join(here, 'kitov-linear.strela'), 'utf8');
      return send(res, 200, { listing });
    }
    if (req.method === 'POST' && url.pathname === '/api/session')
      return send(res, 201, await createFromListing(req));
    if (req.method === 'POST' && url.pathname === '/api/session/import')
      return send(res, 201, await importSnapshot(req));
    const match = /^\/api\/session\/([0-9a-f-]{36})(?:\/(action|snapshot))?$/.exec(url.pathname);
    if (match) {
      const session = sessions.get(match[1]);
      if (!session) fail(404, 'Session not found');
      if (req.method === 'DELETE' && !match[2]) {
        await enqueue(session, () => rm(session.snapshot, { force: true }));
        sessions.delete(match[1]);
        return send(res, 200, { released: true });
      }
      if (req.method === 'GET' && !match[2])
        return send(res, 200, await enqueue(session, () => view(session, url)));
      if (req.method === 'POST' && match[2] === 'action') {
        const input = await jsonBody(req, 4096);
        return send(res, 200, await enqueue(session, async () => {
          await act(session, input);
          return view(session, url);
        }));
      }
      if (req.method === 'GET' && match[2] === 'snapshot') {
        const bytes = await enqueue(session, () => readFile(session.snapshot));
        res.writeHead(200, { 'Content-Type': 'application/octet-stream',
          'Content-Disposition': 'attachment; filename="strela-session.sts"',
          'Content-Length': bytes.length, 'Cache-Control': 'no-store' });
        return res.end(bytes);
      }
    }
    if (req.method === 'GET' && assets.has(url.pathname)) {
      const [name, type] = assets.get(url.pathname);
      const bytes = await readFile(path.resolve(publicDir, name));
      res.writeHead(200, { 'Content-Type': type, 'Content-Length': bytes.length });
      return res.end(bytes);
    }
    fail(404, 'Not found');
  } catch (error) {
    send(res, error.status || 500, { error: error.message || 'Internal error' });
  }
});

server.listen(port, host, () => {
  console.log('Strela operator console: http://' + host + ':' + server.address().port);
  console.log('Using emulator binaries in ' + buildDir);
});
async function close() {
  server.close();
  await rm(tempRoot, { recursive: true, force: true });
}
process.once('SIGINT', close);
process.once('SIGTERM', close);

