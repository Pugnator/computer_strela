import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {encodeUpp, makeUppDeck, makeUppResultDeck, makeUppProgramDeck} from './public/upp.mjs';
// Reference bytes in octal: 0=200, space=217. These distinguish
// a punched zero from the ignored all-zero byte used for unused positions.
assert.deepEqual([...encodeUpp('01 +-')], [0o200,0o001,0o217,0o212,0o013]);
assert.deepEqual([...encodeUpp('АБЯ')].map(x=>x&127),[0x20,0x21,0x3e]);
for (const byte of encodeUpp('0123456789 +-/,. АБВГДЕЖЗИЙКЛМНОПРСТУФХЦЧШЩЫЬЭЮЯ'))
  assert.equal(byte.toString(2).replaceAll('0','').length % 2,1);
assert.throws(()=>encodeUpp('?'));
assert.equal(makeUppDeck('').length,0);
assert.equal(makeUppDeck('1'.repeat(120)).length,1);
const deck=makeUppDeck('1'.repeat(120)+'+2');
assert.equal(deck.length,2);
assert.equal(deck[1].text,'+2');
assert.deepEqual([...deck[1].bytes.slice(0,2)],[0o212,0o002]);
assert.ok(deck[1].bytes.slice(2).every(byte=>byte===0));
assert.equal(deck[0].bytes.length*8,80*12);
console.log('UPP: reference codes, odd parity, card boundary and blank tail PASS');
const records=Array.from({length:29},(_,i)=>'+'+(i+1)+',123456789');
const resultDeck=makeUppResultDeck(records);
assert.equal(resultDeck.map(card=>card.text).join(' '),records.join(' '));
assert.ok(resultDeck.every(card=>card.text.length<=120));
assert.equal(resultDeck.length,4);
console.log('UPP: complete numeric records preserved across cards PASS');
for (const filename of ['kitov-linear.strela','vostok-orbit.strela']) {
  const listing = await readFile(new URL(filename,import.meta.url),'utf8');
  const expected = listing.split(/\r?\n/).map(line=>line.trim())
    .filter(line=>/^[0-7]{4}\s/.test(line)).map(line=>line.split(/\s+/).join(' '));
  const cards = makeUppProgramDeck(listing);
  assert.deepEqual(cards.flatMap(card=>card.listing.split('\n')),expected);
  assert.equal(cards.reduce((count,card)=>count+card.wordCount,0),expected.length);
  assert.ok(cards.every(card=>card.text.length<=120 && card.bytes.length===120));
  for (const card of cards)
    assert.deepEqual(card.bytes.slice(0,card.text.length),encodeUpp(card.text));
}
console.log('UPP: loaded book/orbit program records and card counts PASS');