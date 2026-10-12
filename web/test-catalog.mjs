// Every deck in the card file (картотека перфокарт) must exist, carry the
// fields the page shows, and run to a normal stop in the native emulator.
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const web = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(web, '..');
const runner = path.join(process.env.STRELA_BUILD_DIR || path.join(root, 'build'),
  'strela_run' + (process.platform === 'win32' ? '.exe' : ''));
const { decks } = JSON.parse(await readFile(path.join(web, 'public', 'catalog.json'), 'utf8'));

assert.ok(decks.length >= 9, 'card file has the book programs and the Vostok calculation');
assert.equal(new Set(decks.map((deck) => deck.id)).size, decks.length, 'deck ids are unique');
for (const deck of decks) {
  for (const key of ['id', 'file', 'year', 'book', 'pages', 'title', 'description', 'source', 'href', 'kind'])
    assert.ok(deck[key] !== undefined && deck[key] !== '', `${deck.id}: ${key} present`);
  assert.ok(['book', 'orbit', 'custom'].includes(deck.kind), `${deck.id}: kind`);
  assert.match(deck.file, /^(programs\/)?[a-z0-9-]+\.strela$/, `${deck.id}: file name`);
  const listing = path.join(web, deck.file);
  const result = spawnSync(runner, ['run', listing, '--max-us', '2000000000'], { encoding: 'utf8' });
  assert.equal(result.status, 0, `${deck.id}: normal stop\n${result.stdout}${result.stderr}`);
  assert.match(result.stdout, /^normal stop at /, `${deck.id}: normal stop`);
}
console.log(`Card file: ${decks.length} decks run to a normal stop`);
