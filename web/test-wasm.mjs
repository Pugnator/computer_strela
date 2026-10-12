import assert from 'node:assert/strict';
import { execFileSync, spawnSync } from 'node:child_process';
import { mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import createModule from './dist/strela_wasm.mjs';
import { createWasmBackend } from './public/wasm-api.mjs';

const web = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(web, '..');
const runner = path.join(process.env.STRELA_BUILD_DIR || path.join(root, 'build'),
  'strela_run' + (process.platform === 'win32' ? '.exe' : ''));
const sample = path.join(root, 'examples/programs/two-branches.strela');
const listing = await readFile(sample, 'utf8');
const temp = await mkdtemp(path.join(os.tmpdir(), 'strela-wasm-test-'));
const browser = createWasmBackend(createModule, async () => listing);
const post = (payload) => ({ method: 'POST', body: JSON.stringify(payload) });
await assert.rejects(browser.request('/api/session',
  post({ listing: 'x'.repeat(1024 * 1024 + 1) })), /1 MB/);
await assert.rejects(browser.request('/api/session/import', {
  method: 'POST', body: new ArrayBuffer(16 * 1024 * 1024 + 1)
}), /16 MB/);
try {
  const prepared = await browser.request('/api/session', post({ listing }));
  assert.equal(prepared.state.pc, '0010');
  assert.equal(prepared.state.elapsedUs, '0');
  assert.equal(prepared.state.mainTactUs, 220);
  const initial = browser.exportSnapshot(prepared.id);
  const initialPath = path.join(temp, 'initial.sts');
  const nativeInitial = path.join(temp, 'native-initial.sts');
  await writeFile(initialPath, initial);
  execFileSync(runner, ['prepare', sample, '--save-snapshot', nativeInitial]);
  assert.deepEqual(Buffer.from(initial), await readFile(nativeInitial));

  const stepped = await browser.request('/api/session/' + prepared.id + '/action?ram=0010',
    post({ type: 'step', count: 1 }));
  assert.equal(stepped.pc, '0011');
  assert.equal(stepped.elapsedUs, '500');
  const nativeStep = path.join(temp, 'native-step.sts');
  execFileSync(runner, ['operate', nativeInitial, '--step', '1', '--save-snapshot', nativeStep]);
  assert.deepEqual(Buffer.from(browser.exportSnapshot(prepared.id)), await readFile(nativeStep));

  const runSession = await browser.request('/api/session', post({ listing }));
  const runState = await browser.request('/api/session/' + runSession.id + '/action',
    post({ type: 'run', budgetUs: 1000 }));
  assert.equal(runState.elapsedUs, '1000');
  const nativeRun = path.join(temp, 'native-run.sts');
  const nativeResult = spawnSync(runner, ['resume', nativeInitial, '--max-us', '1000',
    '--save-snapshot', nativeRun], { encoding: 'utf8' });
  assert.ok([0, 1, 3].includes(nativeResult.status), nativeResult.stderr);
  assert.deepEqual(Buffer.from(browser.exportSnapshot(runSession.id)), await readFile(nativeRun));
  assert.deepEqual(await browser.request('/api/session/' + runSession.id,
    { method: 'DELETE' }), { released: true });
  const cards = 'STRELA-CARDS 1\nCARD\n' +
    'WORD 0000 0000 0000 0 00\n'.repeat(12);
  const tape1 = 'STRELA-TAPE 1 UNIT=1 CAPACITY=HISTORICAL-100000\n' +
    'ZONE 4001 1\nWORD 0000 0000 0000 0 01\n';
  const media = await browser.request('/api/session', post({ listing, cards, tape1 }));
  const zone = await browser.request('/api/session/' + media.id + '?zone=4001');
  assert.equal(zone.zone.length, 1);
  assert.equal(zone.zone.words[0], '000000000000001');
  const cardsPath = path.join(temp, 'input.cards');
  const tapePath = path.join(temp, 'input.stt');
  const nativeMedia = path.join(temp, 'native-media.sts');
  await writeFile(cardsPath, cards);
  await writeFile(tapePath, tape1);
  execFileSync(runner, ['prepare', sample, '--cards', cardsPath,
    '--tape1', tapePath, '--save-snapshot', nativeMedia]);
  assert.deepEqual(Buffer.from(browser.exportSnapshot(media.id)), await readFile(nativeMedia));
  const restored = await browser.request('/api/session/import', {
    method: 'POST', body: browser.exportSnapshot(prepared.id)
  });
  assert.equal(restored.state.elapsedUs, '500');
  const reset = await browser.request('/api/session/' + restored.id + '/action',
    post({ type: 'reset', address: '0010' }));
  assert.equal(reset.elapsedUs, '0');
  const orbitListing = await readFile(path.join(web, 'vostok-orbit.strela'), 'utf8');
  const orbit = await browser.request('/api/session', post({ listing: orbitListing }));
  const orbitRun = await browser.request('/api/session/' + orbit.id + '/action?ram=1000',
    post({ type: 'run', budgetUs: 150000000 }));
  assert.equal(orbitRun.reason, 'normal stop');
  assert.equal(orbitRun.punchedWords, 29);
  assert.equal(orbitRun.instructions, '33434');
  const decoded = (raw) => {
    const word = BigInt('0o' + raw);
    const mantissa = Number((word >> 7n) & ((1n << 35n) - 1n)) / 2 ** 35;
    const exponent = Number(word & 63n) * (word & 64n ? -1 : 1);
    return mantissa * 2 ** exponent * (word & (1n << 42n) ? -1 : 1);
  };
  const values = orbitRun.ram.map((entry) => decoded(entry.word));
  assert.ok(Math.abs(values[0] - 181) < 2);
  assert.ok(Math.abs(values[1] - 327) < 2);
  assert.ok(Math.abs(values[2] - 6625) < 2);
  assert.ok(Math.abs(values[3] - 146 / 13250) < 0.001);
  assert.ok(Math.abs(values[6] / 60 - 89.34) < 0.2);
  assert.ok(Math.abs(values[7] - 64.95) < 0.1);
  assert.ok(Math.abs(values[15] - 4725) < 1);
  const orbitTail = await browser.request('/api/session/' + orbit.id + '?ram=1020');
  assert.ok(Math.abs(decoded(orbitTail.ram[0].word) - 66.17) < 2);
  assert.ok(Math.abs(decoded(orbitTail.ram[9].word) - 34.48) < 0.2);
  assert.ok(Math.abs(decoded(orbitTail.ram[10].word) - 243.42) < 0.2);
  assert.ok(Math.abs(decoded(orbitTail.ram[11].word) - 195.26) < 0.2);
  const decimalOrbit = await browser.request('/api/session/' + orbit.id + '?ram=1100');
  const readDecimal = (raw) => {
    const word = BigInt('0o' + raw);
    let mantissa = 0;
    for (let place = 0; place < 9; place++) {
      const digit = Number((word >> BigInt(38 - 4 * place)) & 15n);
      assert.ok(digit <= 9);
      mantissa = mantissa * 10 + digit;
    }
    const exponent = Number(word & 31n) * (word & 32n ? -1 : 1);
    return (word & (1n << 42n) ? -1 : 1) * mantissa / 1e9 * 10 ** exponent;
  };
  assert.ok(Math.abs(readDecimal(decimalOrbit.ram[0].word) - values[0]) < 0.00001);
  assert.ok(Math.abs(readDecimal(decimalOrbit.ram[1].word) - values[1]) < 0.00001);
  const nativeOrbitInitial = path.join(temp, 'orbit-initial.sts');
  const nativeOrbitRun = path.join(temp, 'orbit-run.sts');
  execFileSync(runner, ['prepare', path.join(web, 'vostok-orbit.strela'),
    '--save-snapshot', nativeOrbitInitial]);
  const nativeOrbit = spawnSync(runner, ['resume', nativeOrbitInitial, '--max-us', '150000000',
    '--save-snapshot', nativeOrbitRun], { encoding: 'utf8' });
  assert.equal(nativeOrbit.status, 0, nativeOrbit.stderr);
  assert.deepEqual(Buffer.from(browser.exportSnapshot(orbit.id)), await readFile(nativeOrbitRun));
  const bookListing = await readFile(path.join(web, 'kitov-linear.strela'), 'utf8');
  const book = await browser.request('/api/session', post({listing:bookListing}));
  assert.deepEqual((await browser.request('/api/session/'+book.id+'?cards=0')).words, []);
  const bookRun = await browser.request('/api/session/'+book.id+'/action?ram=0051',
    post({type:'run',budgetUs:1000000}));
  assert.equal(bookRun.reason, 'normal stop');
  assert.equal(bookRun.instructions, '16');
  const bookCards = await browser.request('/api/session/'+book.id+'?cards=0');
  assert.equal(bookCards.total, 2);
  assert.deepEqual(bookCards.words, bookRun.ram.slice(0,2).map(entry=>entry.word));
  assert.equal(readDecimal(bookCards.words[0]), 1);
  assert.equal(readDecimal(bookCards.words[1]), 2);
  assert.deepEqual((await browser.request('/api/session/'+book.id+'?cards=1&count=1')).words,
    [bookCards.words[1]]);
  assert.deepEqual((await browser.request('/api/session/'+book.id+'?cards=2')).words, []);
  await assert.rejects(browser.request('/api/session/'+book.id+'?cards=3'));
  await assert.rejects(browser.request('/api/session/'+book.id+'?cards=0&count=129'));
  const issuedOrbit = await browser.request('/api/session/'+orbit.id+'?cards=0');
  assert.equal(issuedOrbit.words.length, 29);
  assert.deepEqual(issuedOrbit.words.slice(0,16), decimalOrbit.ram.map(entry=>entry.word));
  const viewer = path.join(path.dirname(runner),'strela_view'+(process.platform==='win32'?'.exe':''));
  const nativeCards = JSON.parse(execFileSync(viewer,[nativeOrbitRun,'--cards','0','--count','128'],{encoding:'utf8'}));
  assert.deepEqual(issuedOrbit, nativeCards);
  console.log('Wasm: book, orbit, punched deck, media and snapshots match native CLI');
} finally {
  await rm(temp, { recursive: true, force: true });
}




