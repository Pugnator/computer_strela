// UPP/GOST codes, odd parity, MSB first. 12 rows, 80 positions.
// https://www.mailcom.com/besm6/encoding_ru.html
const alphabet = [
  ...'0123456789', '+', '-', '/', ',', '.', ' ',
  '⏨', '↑', '(', ')', '×', '=', ';', '[', ']', '*', String.fromCharCode(96), "'", '≠', '<', '>', ':',
  ...'АБВГДЕЖЗИЙКЛМНОПРСТУФХЦЧШЩЫЬЭЮЯ', 'D',
  ...'FGIJLNQRSUVWZ', '¯', '≤', '≥', '∨', '∧', '⊃', '¬', '÷', '≡', '%', '◊',
  '|', '—', '_', '!', '"', 'Ъ', '°', '′'
];
const codes = new Map(alphabet.map((character, index) => [character, index]));
for (const [latin, russian] of Object.entries({A:'А',B:'В',C:'С',E:'Е',H:'Н',K:'К',M:'М',O:'О',P:'Р',T:'Т',X:'Х',Y:'У'}))
  codes.set(latin, codes.get(russian));
codes.set('−', codes.get('-'));

export function encodeUpp(text) {
  return Uint8Array.from([...text].map(character => {
    const code = codes.get(character.toUpperCase());
    if (code === undefined) throw new Error('Нет знака в коде УПП: ' + character);
    const ones = code.toString(2).replaceAll('0', '').length;
    return code | (ones % 2 === 0 ? 0x80 : 0);
  }));
}

export function makeUppDeck(text) {
  const bytes = encodeUpp(text);
  const cards = [];
  for (let start = 0; start < bytes.length; start += 120) {
    const data = new Uint8Array(120);
    data.set(bytes.subarray(start, start + 120));
    cards.push({bytes: data, text: [...text].slice(start, start + 120).join('')});
  }
  return cards;
}

const ns = 'http://www.w3.org/2000/svg';
function element(tag, attrs = {}, text) {
  const node = document.createElementNS(ns, tag);
  for (const [key, value] of Object.entries(attrs)) node.setAttribute(key, value);
  if (text !== undefined) node.textContent = text;
  return node;
}

export function renderUppCard(card, number) {
  const svg = element('svg', {viewBox:'0 0 1000 450',role:'img','aria-label':'Перфокарта № ' + number + ', код УПП'});
  const defs = element('defs');
  const filter = element('filter',{id:'card-grain',x:'0',y:'0',width:'100%',height:'100%'});
  filter.append(element('feTurbulence',{type:'fractalNoise',baseFrequency:'.65',numOctaves:'3',seed:'7',stitchTiles:'stitch'}));
  defs.append(filter);
  svg.append(defs);
  svg.append(element('path',{d:'M 48 8 H 986 Q 992 8 992 14 V 436 Q 992 442 986 442 H 14 Q 8 442 8 436 V 76 Z',fill:'#ecd2ad',stroke:'#a99173','stroke-width':'1.5'}));
  svg.append(element('path',{d:'M 48 8 H 986 V 442 H 8 V 76 Z',fill:'#ad7d45',filter:'url(#card-grain)',opacity:'.12'}));
  const print = element('g',{fill:'#7f6a52','font-family':'Courier New, monospace','font-size':'15'});
  const holes = element('g',{fill:'#b1b5b4',stroke:'#756a59','stroke-width':'.7'});
  const x = column => 45 + column * 11.35;
  const y = row => 57 + row * 31.8;
  print.append(element('text',{x:45,y:28,'font-size':12,opacity:'.7'},card.text.slice(0,120)));
  for (let row=0;row<12;row++) {
    for (let column=0;column<80;column++) {
      const bit = row * 80 + column;
      const byte = card.bytes[Math.floor(bit/8)];
      const punched = (byte >> (7 - bit%8)) & 1;
      if (row >= 2)
        print.append(element('text',{x:x(column),y:y(row)+5,'text-anchor':'middle'},String(row-2)));
      if (punched)
        holes.append(element('rect',{x:x(column)-3.2,y:y(row)-9,width:6.4,height:16}));
    }
  }
  for (let column=0;column<80;column++) {
    if (column===0 || (column+1)%2===0)
      for (const baseline of [104,396])
        print.append(element('text',{x:x(column),y:baseline,'text-anchor':'middle','font-size':12},String(column+1)));
  }
  svg.append(print,holes);
  return svg;
}
export function makeUppResultDeck(numbers) {
  const cards = [];
  let text = '';
  for (const number of numbers) {
    if (text && text.length + 1 + number.length > 120) {
      cards.push(...makeUppDeck(text));
      text = '';
    }
    text += (text ? ' ' : '') + number;
  }
  if (text) cards.push(...makeUppDeck(text));
  return cards;
}
export function makeUppProgramDeck(listing) {
  const records = listing.split(/\r?\n/).map(line=>line.trim())
    .filter(line=>/^[0-7]{4}\s/.test(line))
    .map(line=>line.split(/\s+/).join(' '));
  const deck = [];
  let page = [];
  let length = 0;
  function finish() {
    if (!page.length) return;
    const [card] = makeUppDeck(page.join(' '));
    card.listing = page.join('\n');
    card.wordCount = page.length;
    deck.push(card);
    page = [];
    length = 0;
  }
  for (const record of records) {
    if (length + (page.length ? 1 : 0) + record.length > 120) finish();
    page.push(record);
    length += (page.length > 1 ? 1 : 0) + record.length;
  }
  finish();
  return deck;
}