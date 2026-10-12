import assert from 'node:assert/strict';
import { spawn, execFileSync } from 'node:child_process';
import { mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const web = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(web, '..');
const build = process.env.STRELA_BUILD_DIR || path.join(root, 'build');
const runner = path.join(build, 'strela_run' + (process.platform === 'win32' ? '.exe' : ''));
const temp = await mkdtemp(path.join(os.tmpdir(), 'strela-web-test-'));
const child = spawn(process.execPath, [path.join(web, 'server.mjs')], {
  cwd: root, env: { ...process.env, STRELA_PORT: '0', STRELA_BUILD_DIR: build },
  stdio: ['ignore', 'pipe', 'pipe'], windowsHide: true
});

async function startUrl() {
  let output = '';
  return await new Promise((resolve, reject) => {
    const timeout = setTimeout(() => reject(new Error('Web server startup timed out: ' + output)), 5000);
    child.stdout.on('data', (chunk) => {
      output += chunk;
      const found = /http:\/\/127\.0\.0\.1:\d+/.exec(output);
      if (found) { clearTimeout(timeout); resolve(found[0]); }
    });
    child.once('exit', (code) => {
      clearTimeout(timeout);
      reject(new Error('Web server exited early: ' + code + ' ' + output));
    });
  });
}

async function json(url, options) {
  const response = await fetch(url, options);
  const value = await response.json();
  assert.equal(response.ok, true, JSON.stringify(value));
  return value;
}

try {
  const base = await startUrl();
  const page = await fetch(base + '/');
  assert.equal(page.status, 200);
  assert.match(await page.text(), /«Стрела»|пульт управления/i);
  const { listing } = await json(base + '/api/example');
  const created = await json(base + '/api/session', {
    method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ listing })
  });
  assert.equal(created.state.pc, '0020');
  assert.equal(created.state.elapsedUs, '0');
  assert.equal(created.state.mainTactUs, 220);
  assert.equal(created.state.nextInstruction.opcode, '72');
  const id = created.id;
  const initialBytes = Buffer.from(await (await fetch(base + '/api/session/' + id + '/snapshot')).arrayBuffer());
  const initial = path.join(temp, 'initial.sts');
  const cliStep = path.join(temp, 'cli-step.sts');
  await writeFile(initial, initialBytes);
  const stepped = await json(base + '/api/session/' + id + '/action?ram=0020', {
    method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ type: 'step', count: 1 })
  });
  assert.equal(stepped.pc, '0021');
  assert.equal(stepped.elapsedUs, '25000');
  execFileSync(runner, ['operate', initial, '--step', '1', '--save-snapshot', cliStep]);
  const browserStep = Buffer.from(await (await fetch(base + '/api/session/' + id + '/snapshot')).arrayBuffer());
  assert.deepEqual(browserStep, await readFile(cliStep));
  const cards = 'STRELA-CARDS 1\nCARD\n' +
    'WORD 0000 0000 0000 0 00\n'.repeat(12);
  const tape1 = 'STRELA-TAPE 1 UNIT=1 CAPACITY=HISTORICAL-100000\n' +
    'ZONE 4001 1\nWORD 0000 0000 0000 0 01\n';
  const withMedia = await json(base + '/api/session', {
    method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ listing, cards, tape1 })
  });
  const mediaView = await json(base + '/api/session/' + withMedia.id + '?zone=4001');
  assert.equal(mediaView.zone.length, 1);
  assert.equal(mediaView.zone.words[0], '000000000000001');
  const imported = await json(base + '/api/session/import', {
    method: 'POST', headers: { 'Content-Type': 'application/octet-stream' },
    body: browserStep
  });
  assert.equal(imported.state.elapsedUs, '25000');
  const reset = await json(base + '/api/session/' + imported.id + '/action', {
    method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ type: 'reset', address: '0020' })
  });
  assert.equal(reset.elapsedUs, '0');
  assert.equal(reset.pc, '0020');
  console.log('Web console: page, load, media mount, step parity, import, reset passed');
} finally {
  child.kill();
  await rm(temp, { recursive: true, force: true });
}
