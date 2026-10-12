import { makeUppResultDeck, makeUppProgramDeck, renderUppCard } from './upp.mjs';
const $ = (id) => document.getElementById(id);
// web/stamp-dist.py loads this script as app.js?v=<hash>; every file loaded
// at run time carries the same stamp, so a cached old file is never mixed in.
const ASSET_VERSION = new URL(import.meta.url).searchParams.get('v');
const asset = (file) => ASSET_VERSION ? file + '?v=' + ASSET_VERSION : file;
let sessionId = null;
let selectedRam = '0000';
let selectedZone = '';
let busy = false;
let visualStopRequested = false;
let programKind = 'book';
let printerTimer = null;
let printerEpoch = 0;
let printedSession = null;
let punchedCount = 0;
let issuedDeck = [];
let issuedPage = 0;
let programDeck = [];
let viewedDeck = [];
let dialogReturnFocus = null;
let issuedKey = '';
let shownOutputKey = '';
const mediaFiles = { cards: null, tape1: null, tape2: null };
const MAX_LISTING_BYTES = 1024 * 1024;
const MAX_MEDIA_BYTES = 8 * 1024 * 1024;
const MAX_SNAPSHOT_BYTES = 16 * 1024 * 1024;
const textBytes = (value) => new TextEncoder().encode(value).length;
const statusLabels = {
  running: 'Счёт', stopped: 'Останов', fault: 'Аварийный останов',
  budget: 'Останов', paused: 'Готова'
};
const reasonLabels = {
  running: 'Машина готова',
  'normal stop': 'Останов',
  'bad address': 'Неверный адрес',
  'unknown opcode': 'Неизвестный код операции',
  'arithmetic overflow': 'Переполнение',
  'I/O failure': 'Ошибка ввода или выдачи',
  'control stop': 'Контрольный останов',
  'comparison mismatch': 'Несовпадение чисел',
  'unknown constant': 'Неизвестная константа',
  'numeric domain stop': 'Недопустимое число',
  'media absent': 'Нет носителя',
  'media exhausted': 'Конец носителя',
  'checksum mismatch': 'Несовпадение контрольных сумм',
  'simulated time overflow': 'Переполнение счётчика времени'
};
const reasonText = (reason) => reasonLabels[reason] || 'Неизвестный останов';
const russianError = (message) => /[А-Яа-яЁё]/.test(message) ? message : 'Не удалось выполнить операцию.';

function notice(message, error = false) {
  $('notice').textContent = message;
  $('notice').classList.toggle('error', error);
}

let backendPromise;
function getBackend() {
  if (!backendPromise) backendPromise = (async () => {
    const probe = await fetch(asset('./strela_wasm.mjs'), { method: 'HEAD' });
    if (!probe.ok) {
      return null;
    }
    const [{ default: createModule }, { createWasmBackend }] = await Promise.all([
      import(asset('./strela_wasm.mjs')), import(asset('./wasm-api.mjs'))
    ]);
    const versionedModule = (options) =>
      createModule({ ...options, locateFile: (file) => asset('./' + file) });
    return createWasmBackend(versionedModule, async () => {
      const response = await fetch(asset('./kitov-linear.strela'));
      if (!response.ok) throw new Error('Программа из книги недоступна');
      return response.text();
    });
  })();
  return backendPromise;
}

async function request(url, options) {
  const backend = await getBackend();
  if (backend) return backend.request(url, options);
  const response = await fetch(url, options);
  let data;
  try { data = await response.json(); }
  catch { throw new Error('Нет ответа от машины'); }
  if (!response.ok) throw new Error(data.error || 'Не удалось выполнить операцию');
  return data;
}
function stateQuery() {
  const query = new URLSearchParams({ ram: selectedRam });
  if (selectedZone) query.set('zone', selectedZone);
  return '?' + query;
}

async function work(operation) {
  if (busy) return;
  busy = true;
  for (const button of document.querySelectorAll('button')) button.disabled = true;
  try { await operation(); }
  catch (error) { notice(russianError(error.message), true); }
  finally {
    busy = false;
    for (const button of document.querySelectorAll('button'))
      button.disabled = button.id === 'haltVisual' ||
        (!sessionId && !['loadListing', 'openCatalog', 'clearMedia'].includes(button.id) &&
         !button.closest('dialog'));
    $('deckVisual').disabled = programDeck.length === 0;
    updateOutputControls();
    if ($('outputDialog').open) $('closeOutputDeck').focus();
  }
}

function fields(raw) {
  if (!raw) return 'нет слова';
  const word = BigInt('0o' + raw);
  const a = Number((word >> 31n) & 0o7777n).toString(8).padStart(4, '0');
  const b = Number((word >> 19n) & 0o7777n).toString(8).padStart(4, '0');
  const c = Number((word >> 7n) & 0o7777n).toString(8).padStart(4, '0');
  return a + ' · ' + b + ' · ' + c + ' · ' + Number((word >> 6n) & 1n) + ' · ' +
    Number(word & 0o77n).toString(8).padStart(2, '0');
}

function set(id, value) { const element = $(id); if (!element) throw new Error('Не найден элемент: ' + id); element.textContent = String(value); }
function commandCount(value) {
  const count = BigInt(value);
  const last = count % 10n;
  const lastTwo = count % 100n;
  const phrase = last === 1n && lastTwo !== 11n ? 'команда выполнена' :
    last >= 2n && last <= 4n && (lastTwo < 12n || lastTwo > 14n) ? 'команды выполнены' :
    'команд выполнено';
  return count.toLocaleString('ru-RU') + ' ' + phrase;
}
function showLamps(raw) {
  const strip = $('nextRaw');
  strip.setAttribute('aria-label', 'Восьмеричная запись команды: ' + raw);
  strip.replaceChildren(...[...raw].map((digit) => {
    const lamp = document.createElement('span');
    lamp.className = 'digit-tube';
    lamp.textContent = digit;
    return lamp;
  }));
}

function render(state) {
  const stopped = state.reason !== 'running';
  set('statusText', stopped ? (statusLabels[state.status] || 'Останов') : 'Готова');
  $('statusLamp').className = 'lamp ' + (stopped ? (state.status === 'fault' ? 'fault' : 'stopped') : 'running');
  set('pc', state.pc);
  set('omega', state.omega);
  set('elapsed', BigInt(state.elapsedUs).toLocaleString('ru-RU') + ' мкс');
  set('intervals', commandCount(state.instructions));
  set('reason', reasonText(state.reason));
  set('displayA', state.displayA);
  set('displayB', state.displayB);
  $('checkStops').checked = state.checkStops;
  set('nextAt', state.pc);
  set('lastAt', state.lastLocation);
  set('lastRaw', state.lastInstruction.raw);
  const next = state.nextInstruction;
  showLamps(next?.raw || '---------------');
  set('fieldA', next?.a || '----');
  set('fieldB', next?.b || '----');
  set('fieldC', next?.c || '----');
  set('fieldCheck', next?.check ?? '–');
  set('fieldOp', next?.opcode || '--');
  const bits = next ? BigInt('0o' + next.raw).toString(2).padStart(43, '0') : '0'.repeat(43);
  $('bitStrip').replaceChildren(...[...bits].map((bit) => {
    const element = document.createElement('span');
    element.className = 'bit' + (bit === '1' ? ' on' : '');
    return element;
  }));
  set('mainTact', state.mainTactUs + ' мкс');
  set('cardsRead', state.cardPosition);
  set('cardsPunched', state.punchedWords);
  punchedCount = Number(state.punchedWords);
  $('outputCard').hidden = punchedCount === 0;
  set('tape1', state.tapeSupplied[0]);
  set('tape2', state.tapeSupplied[1]);

  $('ramRows').replaceChildren(...state.ram.map((entry) => {
    const row = document.createElement('tr');
    for (const value of [entry.address, entry.word || 'нет слова', fields(entry.word)]) {
      const cell = document.createElement('td');
      cell.textContent = value;
      row.append(cell);
    }
    row.addEventListener('click', () => {
      $('ramRows').querySelectorAll('tr').forEach((item) => item.classList.remove('selected'));
      row.classList.add('selected');
      $('editAddress').value = entry.address;
      if (entry.word) $('editWord').value = entry.word;
    });
    return row;
  }));
  if (state.zone) {
    const zone = state.zone;
    $('zoneOutput').textContent = 'ЗОНА ' + zone.address + ' · ' + zone.length +
      ' слов\n' + (zone.words.length ? zone.words.map((word, i) =>
        i.toString(8).padStart(4, '0') + '  ' + word).join('\n') : 'Зона пуста или лента не установлена');
  } else {
    $('zoneOutput').textContent = 'Для просмотра слов укажите адрес зоны первой или второй ленты.';
  }
}

async function refresh() {
  if (!sessionId) return;
  render(await request('/api/session/' + sessionId + stateQuery()));
}


function showProgramDescription(kind) {
  const plate = $('introPlate');
  const title = $('bookTitle');
  const description = $('bookDescription');
  const source = $('sourceLink');
  if (kind && typeof kind === 'object') {
    plate.textContent = 'ПРОГРАММА ИЗ КНИГИ · ' + kind.year;
    title.textContent = kind.title;
    description.textContent = kind.description + ' Источник: ' + kind.source + ', ' + kind.pages + '.';
    source.href = kind.href;
    source.textContent = 'Книга ' + kind.year + ' года ↗';
    return;
  }
  if (kind === 'orbit') {
    plate.textContent = 'РАСЧЁТНАЯ ПРОГРАММА · «ВОСТОК-1»';
    title.textContent = 'Выведение корабля на орбиту';
    description.textContent = 'По стартовой массе 287 т, тяге, расходу топлива и времени работы ступеней машина рассчитывает выведение, затем получает высоты перигея и апогея, наклонение, период и скорости.';
    source.href = 'https://ru.djvu.online/file/c5OWKbCfqjM34';
    source.textContent = 'Отчёт о полёте ↗';
  } else if (kind === 'book') {
    plate.textContent = 'ПРОГРАММА ИЗ КНИГИ · 1958';
    title.textContent = 'Решение двух линейных уравнений';
    description.textContent = 'А. И. Китов и Н. А. Криницкий, «Электронные вычислительные машины», таблица 4, с. 45–46. Для уравнений x + 2y = 5 и 7x + 5y = 17 машина находит x ≈ 1 и y ≈ 2. Ответ записывается в ячейки 0051 и 0052, затем выдаётся на перфокарту.';
    source.href = 'https://www.computer-museum.ru/books/kitov_evm.pdf';
    source.textContent = 'Книга Китова и Криницкого ↗';
  } else {
    plate.textContent = 'ПРОГРАММА ОПЕРАТОРА';
    title.textContent = 'Программа для машины «Стрела»';
    description.textContent = 'Команды и числа записываются в ячейки памяти в указанном порядке.';
    source.href = 'https://www.computer-museum.ru/books/kitov_evm.pdf';
    source.textContent = 'Описание машины ↗';
  }
}

function clearPrinter() {
  printerEpoch++;
  if (printerTimer) clearInterval(printerTimer);
  printerTimer = null;
  printedSession = null;
  punchedCount = 0;
  issuedDeck = [];
  issuedKey = '';
  shownOutputKey = '';
  $('outputDialog').close();
  $('outputCard').hidden = true;
  printerPaper.replaceChildren();
  set('printerStatus', 'Колода с результатами ожидается.');
}

function updateOutputControls() {
  $('previousOutputCard').disabled = issuedPage === 0;
  $('nextOutputCard').disabled = issuedPage + 1 >= viewedDeck.length;
}

function displayOutputCard() {
  const card = viewedDeck[issuedPage];
  if (!card) return;
  set('outputCardNumber', 'Перфокарта ' + (issuedPage + 1) + ' из ' + viewedDeck.length);
  $('outputCardImage').replaceChildren(renderUppCard(card, issuedPage + 1));
  set('outputCardText', card.listing || card.text);
  updateOutputControls();
}

async function showIssuedDeck(automatic = false) {
  if (!sessionId || !punchedCount) return;
  const id = sessionId;
  const key = id + ':' + punchedCount;
  if (automatic && shownOutputKey === key) return;
  if (issuedKey !== key) {
    const words = [];
    for (let start = 0; start < punchedCount; start += 128) {
      const page = await request('/api/session/' + id + '?cards=' + start + '&count=128');
      words.push(...page.words);
    }
    if (id !== sessionId) return;
    const numbers = words.map(word => {
      if (programKind === 'custom') return word;
      try {
        const value = decimalWord(word).replaceAll(' ', '').replace('−', '-');
        return value.startsWith('-') ? value : '+' + value;
      } catch { return word; }
    });
    issuedDeck = makeUppResultDeck(numbers);
    issuedKey = key;
    issuedPage = 0;
  }
  shownOutputKey = key;
  viewedDeck = issuedDeck;
  issuedPage = 0;
  dialogReturnFocus = $('showOutputDeck');
  set('outputDialogTitle', issuedDeck.length === 1 ? 'Выданная перфокарта' : 'Колода результатов');
  displayOutputCard();
  if (!$('outputDialog').open) $('outputDialog').showModal();
  $('closeOutputDeck').focus();
}
function showProgramDeck() {
  if (!programDeck.length || busy) return;
  viewedDeck = programDeck;
  issuedPage = 0;
  dialogReturnFocus = $('deckVisual');
  set('outputDialogTitle', 'Колода программы');
  displayOutputCard();
  $('outputDialog').showModal();
  $('closeOutputDeck').focus();
}

async function feedDeck(deck) {
  const count = deck.length;
  set('deckCount', count + ' карт в колоде');
  set('deckRead', '0 слов прочитано');
  $('deckProgress').style.width = '0%';
  let read = 0;
  for (let card = 0; card < count; card++) {
    const first = deck[card].listing.split('\n')[0];
    set('deckCurrent', 'КАРТА ' + String(card + 1).padStart(2, '0') + ' · ' + first);
    $('deckVisual').classList.add('feeding');
    await new Promise(resolve => setTimeout(resolve, Math.min(60, 1800 / count)));
    $('deckVisual').classList.remove('feeding');
    read += deck[card].wordCount;
    set('deckRead', read + ' слов прочитано');
    $('deckProgress').style.width = ((card + 1) / count * 100) + '%';
  }
  set('deckCurrent', 'КОЛОДА ПРОЧИТАНА');
}

function decimalWord(raw) {
  if (!raw) throw new Error('Нет числа в колоде результатов');
  const word = BigInt('0o' + raw);
  let digits = '';
  for (let place = 0; place < 9; place++) {
    const digit = Number((word >> BigInt(38 - 4 * place)) & 15n);
    if (digit > 9) throw new Error('Неверная десятичная запись результата');
    digits += digit;
  }
  if (digits === '000000000') return '0';
  const exponent = Number(word & 31n) * ((word & 32n) ? -1 : 1);
  if (exponent < -19 || exponent > 19)
    throw new Error('Неверный порядок десятичного числа');
  let integer;
  let fraction = '';
  if (exponent <= 0) {
    integer = '0';
    fraction = '0'.repeat(-exponent) + digits;
  } else if (exponent < 9) {
    integer = digits.slice(0, exponent);
    fraction = digits.slice(exponent);
  } else {
    integer = digits + '0'.repeat(exponent - 9);
  }
  fraction = fraction.replace(/0+$/, '');
  integer = integer.replace(/\B(?=(\d{3})+(?!\d))/g, ' ');
  return (word & (1n << 42n) ? '−' : '') + integer +
    (fraction ? ',' + fraction : '');
}
const orbitNames = [
  'Высота перигея, км', 'Высота апогея, км', 'Большая полуось, км',
  'Эксцентриситет', 'Параметр орбиты, км', 'Малая полуось, км',
  'Период обращения, с', 'Наклонение, градусы',
  'Скорость в перигее, км/с', 'Скорость в апогее, км/с',
  'Радиус при отделении, км', 'Высота при отделении, км',
  'Радиальная скорость, км/с', 'Касательная скорость, км/с',
  'Полная скорость, км/с', 'Масса корабля, кг',
  'Высота после боковых блоков, км', 'Высота после центрального блока, км',
  'Путь на участке выведения, км', 'Удельная энергия, км²/с²',
  'Удельный момент, км²/с', 'Угол пути при отделении, градусы',
  'Среднее движение, рад/с', 'Радиус перигея, км', 'Радиус апогея, км',
  'Долгота восходящего узла, градусы', 'Аргумент перигея, градусы',
  'Средняя аномалия, градусы', 'Истинная аномалия, градусы'
];

async function printOrbit() {
  if (printedSession === sessionId || !sessionId) return;
  const id = sessionId;
  const deck = await request('/api/session/' + id + '?cards=0&count=128');
  if (id !== sessionId || deck.words.length < orbitNames.length)
    throw new Error('Колода результатов не выдана');
  const numbers = deck.words.slice(0, orbitNames.length).map(decimalWord);
  const paper = $('printerPaper');
  paper.replaceChildren();
  const values = numbers.map((_, i) => {
    const row = document.createElement('div');
    row.className = 'printer-row';
    const label = document.createElement('span');
    label.className = 'printer-label';
    label.textContent = String(i + 1).padStart(2, '0') + '. ' + orbitNames[i];
    const value = document.createElement('span');
    value.className = 'printer-value printer-ink';
    row.append(label, value);
    paper.append(row);
    return value;
  });
  printedSession = id;
  const epoch = ++printerEpoch;
  const total = numbers.reduce((sum, number) => sum + number.length, 0);
  let position = 0;
  let rowIndex = 0;
  let column = 0;
  let carry = 0;
  let last = performance.now();
  set('printerStatus', 'Печатание числовой таблицы: 0 из ' + total + ' знаков.');
  printerTimer = setInterval(() => {
    if (epoch !== printerEpoch) { clearInterval(printerTimer); return; }
    const now = performance.now();
    const rate = Math.max(1, Math.min(200, Number($('printerRate').value) || 40));
    carry += (now - last) * rate / 1000;
    last = now;
    let count = Math.min(Math.floor(carry), total - position);
    carry -= count;
    while (count > 0) {
      const take = Math.min(count, numbers[rowIndex].length - column);
      values[rowIndex].textContent += numbers[rowIndex].slice(column, column + take);
      position += take;
      column += take;
      count -= take;
      if (column === numbers[rowIndex].length) { rowIndex++; column = 0; }
    }
    set('printerStatus', 'Печатание числовой таблицы: ' + position + ' из ' + total + ' знаков.');
    if (position === total) {
      clearInterval(printerTimer);
      printerTimer = null;
      set('printerStatus', 'Таблица перепечатана с колоды результатов.');
    }
  }, 25);
}
async function createSession(listing, catalogDeck = null) {
  if (typeof listing !== 'string' || listing.length > MAX_LISTING_BYTES || textBytes(listing) > MAX_LISTING_BYTES)
    throw new Error('Программа превышает предел 1 Мбайт.');
  const mediaSize = Object.values(mediaFiles).reduce((sum, file) => sum + (file?.size || 0), 0);
  if (mediaSize > 16 * 1024 * 1024)
    throw new Error('Общий размер перфокарт и лент превышает 16 Мбайт.');
  programKind = catalogDeck ? catalogDeck.kind : listing.includes('Расчёт выведения корабля «Восток»') ? 'orbit' :
    listing.includes('Решение системы двух линейных уравнений.') ? 'book' : 'custom';
  showProgramDescription(catalogDeck && !['linear-1958', 'vostok'].includes(catalogDeck.id) ? catalogDeck : programKind);
  currentDeckId = catalogDeck ? catalogDeck.id : programKind === 'book' ? 'linear-1958' :
    programKind === 'orbit' ? 'vostok' : null;
  clearPrinter();
  $('budget').value = programKind === 'orbit' ? '150000000' : '1000000';
  const deck = makeUppProgramDeck(listing);
  await feedDeck(deck);
  const payload = { listing };
  for (const [key, file] of Object.entries(mediaFiles))
    if (file) {
      if (file.size > MAX_MEDIA_BYTES) throw new Error('Носитель превышает предел 8 Мбайт.');
      payload[key] = await file.text();
    }
  const result = await request('/api/session', {
    method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload)
  });
  const previousSession = sessionId;
  programDeck = deck;
  sessionId = result.id;
  selectedRam = result.state.pc;
  $('ramAddress').value = selectedRam;
  $('resetAddress').value = result.state.pc;
  await refresh();
  if (previousSession) await request('/api/session/' + previousSession, { method: 'DELETE' });
  notice('Программа записана. Адрес первой команды: ' + result.state.pc + '.');
}
async function action(input, message) {
  if (!sessionId) throw new Error('Сначала запишите программу');
  const state = await request('/api/session/' + sessionId + '/action' + stateQuery(), {
    method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(input)
  });
  render(state);
  notice(message + ' · ' + reasonText(state.reason) + ' · ' + BigInt(state.elapsedUs).toLocaleString('ru-RU') + ' мкс');
  if (state.reason === 'normal stop') {
    if (programKind === 'orbit') await printOrbit();
    await showIssuedDeck(true);
  }
}

function slowdownMs() {
  const value = Number($('slowdown').value);
  if (!Number.isInteger(value) || value < 0 || value > 1000)
    throw new Error('Укажите замедление от 0 до 1000 мс на шаг.');
  return value;
}

async function runVisual(budgetUs) {
  if (!sessionId) throw new Error('Сначала введите программу в память');
  if (budgetUs !== null && (!Number.isSafeInteger(budgetUs) || budgetUs < 1 || budgetUs > 1000000000))
    throw new Error('Укажите время счёта от 1 до 1000000000 мкс.');
  const id = sessionId;
  const startState = await request('/api/session/' + id + stateQuery());
  const startUs = BigInt(startState.elapsedUs);
  const budget = budgetUs === null ? null : BigInt(budgetUs);
  let state = startState;
  visualStopRequested = false;
  $('haltVisual').disabled = false;
  notice(slowdownMs() ? 'Счёт с замедлением…' : 'Счёт…');
  try {
    while (!visualStopRequested && state.reason === 'running' &&
           (budget === null || BigInt(state.elapsedUs) - startUs < budget)) {
      const delay = slowdownMs();
      const remaining = budget === null ? 100000n : budget - (BigInt(state.elapsedUs) - startUs);
      state = await request('/api/session/' + id + '/action' + stateQuery(), {
        method: 'POST', headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(delay === 0 ?
          { type: 'run', budgetUs: Number(remaining < 100000n ? remaining : 100000n) } :
          { type: 'step', count: 1 })
      });
      render(state);
      if (state.reason === 'running') {
        set('statusText', 'Счёт');
        set('reason', 'Идёт счёт');
      }
      if (state.reason !== 'running' || (budget !== null && BigInt(state.elapsedUs) - startUs >= budget))
        break;
      await new Promise((resolve) => setTimeout(resolve, delay));
    }
    if (state.reason === 'normal stop') {
      notice('Счёт завершён · ' + commandCount(state.instructions) + '.');
      if (programKind === 'orbit') await printOrbit();
      await showIssuedDeck(true);
    } else if (visualStopRequested) {
      set('statusText', 'Останов');
      set('reason', 'Останов оператором');
      notice('Счёт остановлен оператором.');
    } else if (state.reason === 'running') {
      set('statusText', 'Останов');
      set('reason', 'Предел времени счёта');
      notice('Заданное время счёта истекло.');
    } else {
      notice('Счёт завершён · ' + reasonText(state.reason) + '.');
    }
  } finally {
    $('haltVisual').disabled = true;
    visualStopRequested = false;
  }
}

function octal4(value) { return /^[0-7]{4}$/.test(value); }
function octalWord(value) { return /^[0-7]{15}$/.test(value) && BigInt('0o' + value) <= 0x7ffffffffffn; }

$('step').addEventListener('click', () => work(() => action({ type: 'step', count: 1 }, 'Выполнена одна операция')));
$('step10').addEventListener('click', () => work(() => action({ type: 'step', count: 10 }, 'Выполнено до десяти операций')));
$('run').addEventListener('click', () => work(async () => {
  await runVisual($('timeLimit').checked ? Number($('budget').value) : null);
}));
$('timeLimit').addEventListener('change', () => { $('budget').disabled = !$('timeLimit').checked; });
$('haltVisual').addEventListener('click', () => {
  visualStopRequested = true;
  $('haltVisual').disabled = true;
});
$('reset').addEventListener('click', () => work(async () => {
  const address = $('resetAddress').value.trim();
  if (!octal4(address)) throw new Error('Адрес начала должен содержать четыре восьмеричные цифры');
  clearPrinter();
  await action({ type: 'reset', address }, 'Выполнен начальный пуск');
}));
$('checkStops').addEventListener('change', () => work(() =>
  action({ type: 'check', enabled: $('checkStops').checked }, 'Положение контрольного останова изменено')));
$('inspectRam').addEventListener('click', () => work(async () => {
  const value = $('ramAddress').value.trim();
  if (!octal4(value) || parseInt(value, 8) > 0o3777) throw new Error('Адрес ячейки должен быть от 0000 до 3777');
  selectedRam = value;
  await refresh();
  notice('Показаны ячейки с адреса ' + value);
}));
$('writeWord').addEventListener('click', () => work(async () => {
  const address = $('editAddress').value.trim();
  const word = $('editWord').value.trim();
  if (!octal4(address) || !octalWord(word)) throw new Error('Укажите четырёхзначный адрес и пятнадцатизначное восьмеричное слово');
  await action({ type: 'set', address, word }, 'Слово записано в ячейку ' + address);
}));
$('inspectZone').addEventListener('click', () => work(async () => {
  const value = $('zoneAddress').value.trim();
  if (!/^[45][0-7]{3}$/.test(value) || /^[45]000$/.test(value)) throw new Error('Укажите зону от 4001 до 4777 или от 5001 до 5777');
  selectedZone = value;
  await refresh();
  notice('Показана зона ленты ' + value);
}));
$('loadListing').addEventListener('click', () => work(() => createSession($('listing').value)));
// Card file (картотека перфокарт): decks of programs in a drawer.
let catalog = null;
let catalogSort = 'book';
let currentDeckId = 'linear-1958';
let choosingDeck = false;
const sortLabels = { book: 'по книгам', title: 'по названию', size: 'по числу карт' };
const reduceMotion = () => window.matchMedia('(prefers-reduced-motion: reduce)').matches;

function cardsLabel(count) {
  const last = count % 10, lastTwo = count % 100;
  const word = last === 1 && lastTwo !== 11 ? 'карта' :
    last >= 2 && last <= 4 && (lastTwo < 12 || lastTwo > 14) ? 'карты' : 'карт';
  return count + ' ' + word;
}

async function loadCatalog() {
  if (catalog) return catalog;
  const response = await fetch(asset('./catalog.json'));
  if (!response.ok) throw new Error('Картотека перфокарт недоступна.');
  const data = await response.json();
  await Promise.all(data.decks.map(async (deck, index) => {
    const listing = await fetch(asset('./' + deck.file));
    if (!listing.ok) throw new Error('Колода «' + deck.title + '» недоступна.');
    deck.listing = await listing.text();
    deck.cards = makeUppProgramDeck(deck.listing).length;
    deck.order = index;
  }));
  catalog = data;
  return catalog;
}

function sortedDecks() {
  const decks = [...catalog.decks];
  if (catalogSort === 'title') decks.sort((a, b) => a.title.localeCompare(b.title, 'ru'));
  else if (catalogSort === 'size') decks.sort((a, b) => a.cards - b.cards || a.order - b.order);
  else decks.sort((a, b) => a.order - b.order);
  return decks;
}

function deckElement(deck) {
  const item = document.createElement('li');
  item.dataset.id = deck.id;
  const button = document.createElement('button');
  button.type = 'button';
  button.className = 'catalog-deck' + (deck.id === currentDeckId ? ' current' : '');
  button.style.setProperty('--depth', Math.min(14, 3 + Math.round(deck.cards / 4)) + 'px');
  const tab = document.createElement('span');
  tab.className = 'catalog-tab';
  tab.textContent = deck.book === 'vostok' ? 'РАСЧЁТ · ВОСТОК-1' : 'КНИГА ' + deck.year + ' · ' + deck.pages;
  const title = document.createElement('strong');
  title.textContent = deck.title;
  const text = document.createElement('span');
  text.className = 'catalog-text';
  text.textContent = deck.description;
  const meta = document.createElement('span');
  meta.className = 'catalog-meta';
  const count = document.createElement('span');
  count.textContent = cardsLabel(deck.cards);
  const state = document.createElement('span');
  state.textContent = deck.id === currentDeckId ? 'В считывающем устройстве' : 'Вынуть колоду';
  meta.append(count, state);
  button.append(tab, title, text, meta);
  button.addEventListener('click', () => chooseDeck(deck, button));
  item.append(button);
  return item;
}

function renderCatalog(animate) {
  const list = $('catalogList');
  list.classList.remove('riffle', 'choosing');
  list.replaceChildren(...sortedDecks().map(deckElement));
  list.querySelectorAll('.catalog-deck').forEach((button, i) => button.style.setProperty('--i', i));
  if (animate) {
    void list.offsetWidth;
    list.classList.add('riffle');
  }
}

function resortCatalog(sort) {
  if (!catalog || sort === catalogSort || choosingDeck) return;
  catalogSort = sort;
  for (const button of document.querySelectorAll('.catalog-sort button'))
    button.setAttribute('aria-pressed', String(button.dataset.sort === sort));
  const list = $('catalogList');
  list.classList.remove('riffle');
  const items = [...list.children];
  const before = new Map(items.map(item => [item.dataset.id, item.getBoundingClientRect()]));
  const byId = new Map(items.map(item => [item.dataset.id, item]));
  const ordered = sortedDecks().map(deck => byId.get(deck.id));
  list.append(...ordered);
  ordered.forEach((item, i) => item.querySelector('.catalog-deck').style.setProperty('--i', i));
  set('catalogStatus', 'Колоды разложены ' + sortLabels[sort] + '.');
  if (reduceMotion()) {
    // Gentle version: the decks are dealt again in their new order.
    ordered.forEach((item, i) => item.animate(
      [{ opacity: 0, transform: 'translateY(6px)' }, { opacity: 1, transform: 'none' }],
      { duration: 260, delay: i * 70, easing: 'ease-out', fill: 'backwards' }));
    return;
  }
  // Each deck is lifted out of its old place and dropped into the new one,
  // one after another, like cards passing through a sorting machine.
  ordered.forEach((item, i) => {
    const from = before.get(item.dataset.id);
    const to = item.getBoundingClientRect();
    const dx = from.left - to.left, dy = from.top - to.top;
    if (!dx && !dy) return;
    item.animate([
      { transform: `translate(${dx}px, ${dy}px)` },
      { transform: `translate(${dx * 0.5}px, ${dy * 0.5 - 40}px) rotate(${i % 2 ? 5 : -5}deg)`, offset: 0.45 },
      { transform: 'none' }
    ], { duration: 560, delay: i * 50, easing: 'cubic-bezier(.3,.7,.3,1)', fill: 'backwards' });
  });
}

async function chooseDeck(deck, button) {
  if (busy || choosingDeck) return;
  choosingDeck = true;
  const list = $('catalogList');
  list.classList.remove('riffle');
  list.classList.add('choosing');
  button.classList.add('chosen');
  set('catalogStatus', 'Колода «' + deck.title + '» вынимается из ящика.');
  try {
    if (reduceMotion()) {
      await button.animate([
        { transform: 'none', opacity: 1 },
        { transform: 'translateY(-8px)', opacity: 0 }
      ], { duration: 360, easing: 'ease-in', fill: 'forwards' }).finished;
    } else {
      await button.animate([
        { transform: 'none' },
        { transform: 'translateY(-38px) rotate(-2deg)', offset: 0.35 },
        { transform: 'translateY(-38px) rotate(-2deg)', offset: 0.55 },
        { transform: 'translate(40px, -110vh) rotate(7deg)', opacity: 0.2 }
      ], { duration: 760, easing: 'cubic-bezier(.45,0,.7,.2)', fill: 'forwards' }).finished;
    }
  } finally {
    choosingDeck = false;
  }
  $('catalogDialog').close();
  $('listing').value = deck.listing;
  await work(() => createSession(deck.listing, deck));
}

$('openCatalog').addEventListener('click', async () => {
  await work(loadCatalog);
  if (!catalog || $('catalogDialog').open) return;
  renderCatalog(true);
  set('catalogStatus', 'В ящике ' + catalog.decks.length + ' колод. Выберите программу.');
  $('catalogDialog').showModal();
  const current = $('catalogList').querySelector('.catalog-deck.current') ||
    $('catalogList').querySelector('.catalog-deck');
  if (current) current.focus({ preventScroll: true });
});
$('closeCatalog').addEventListener('click', () => $('catalogDialog').close());
$('catalogDialog').addEventListener('close', () => {
  if (!$('openCatalog').disabled) $('openCatalog').focus();
});
for (const button of document.querySelectorAll('.catalog-sort button'))
  button.addEventListener('click', () => resortCatalog(button.dataset.sort));
const mediaLabels = { cards: 'Перфокарты', tape1: 'Лента 1', tape2: 'Лента 2' };
function updateMediaNames() {
  const selected = Object.entries(mediaFiles).filter(([, file]) => file)
    .map(([key, file]) => mediaLabels[key] + ': ' + file.name);
  set('mediaNames', selected.length ? selected.join(' · ') : 'Перфокарты и ленты не выбраны');
}
for (const key of Object.keys(mediaFiles)) {
  $(key + 'File').addEventListener('change', (event) => {
    const file = event.target.files[0] || null;
    if (file && file.size > MAX_MEDIA_BYTES) {
      mediaFiles[key] = null;
      event.target.value = '';
      notice(mediaLabels[key] + ': файл превышает предел 8 Мбайт.', true);
    } else {
      mediaFiles[key] = file;
    }
    updateMediaNames();
  });
}
$('clearMedia').addEventListener('click', () => {
  for (const key of Object.keys(mediaFiles)) {
    mediaFiles[key] = null;
    $(key + 'File').value = '';
  }
  updateMediaNames();
});
$('listingFile').addEventListener('change', async (event) => {
  const file = event.target.files[0];
  event.target.value = '';
  if (!file) return;
  if (file.size > MAX_LISTING_BYTES) {
    notice('Программа превышает предел 1 Мбайт.', true);
    return;
  }
  try { $('listing').value = await file.text(); }
  catch { notice('Не удалось открыть программу.', true); }
});
$('snapshotFile').addEventListener('change', (event) => work(async () => {
  const file = event.target.files[0];
  if (!file) return;
  event.target.value = '';
  if (file.size > MAX_SNAPSHOT_BYTES)
    throw new Error('Состояние превышает предел 16 Мбайт.');
  const result = await request('/api/session/import', {
    method: 'POST', headers: { 'Content-Type': 'application/octet-stream' }, body: await file.arrayBuffer()
  });
  const previousSession = sessionId;
  clearPrinter();
  programKind = 'custom';
  programDeck = [];
  showProgramDescription(programKind);
  sessionId = result.id;
  selectedRam = result.state.pc;
  $('ramAddress').value = selectedRam;
  $('resetAddress').value = result.state.pc;
  await refresh();
  if (previousSession) await request('/api/session/' + previousSession, { method: 'DELETE' });
  notice('Состояние восстановлено. Адрес команды: ' + result.state.pc);
  event.target.value = '';
}));
$('saveSnapshot').addEventListener('click', () => work(async () => {
  if (!sessionId) return;
  const backend = await getBackend();
  if (!backend) {
    window.location.href = '/api/session/' + sessionId + '/snapshot';
    return;
  }
  const bytes = backend.exportSnapshot(sessionId);
  const url = URL.createObjectURL(new Blob([bytes], { type: 'application/octet-stream' }));
  const link = document.createElement('a');
  link.href = url;
  link.download = 'strela-sostoyanie.sts';
  document.body.append(link);
  link.click();
  link.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
  notice('Состояние сохранено.');
}));
for (const [input, button] of [['ramAddress', 'inspectRam'], ['zoneAddress', 'inspectZone']]) {
  $(input).addEventListener('keydown', (event) => { if (event.key === 'Enter') $(button).click(); });
}

$('deckVisual').addEventListener('click', showProgramDeck);
$('showOutputDeck').addEventListener('click', () => work(() => showIssuedDeck()));
$('closeOutputDeck').addEventListener('click', () => $('outputDialog').close());
$('outputDialog').addEventListener('close', () => {
  if (dialogReturnFocus && !dialogReturnFocus.disabled) dialogReturnFocus.focus();
});
$('previousOutputCard').addEventListener('click', () => {
  if (issuedPage > 0) { issuedPage--; displayOutputCard(); }
});
$('nextOutputCard').addEventListener('click', () => {
  if (issuedPage + 1 < viewedDeck.length) { issuedPage++; displayOutputCard(); }
});
$('outputDialog').addEventListener('keydown', event => {
  if (event.key === 'ArrowLeft' && issuedPage > 0) {
    event.preventDefault(); issuedPage--; displayOutputCard();
  } else if (event.key === 'ArrowRight' && issuedPage + 1 < viewedDeck.length) {
    event.preventDefault(); issuedPage++; displayOutputCard();
  }
});
function closeDrawer(drawer) {
  if (!drawer) return;
  drawer.open = false;
  drawer.querySelector('summary').focus();
}
for (const button of document.querySelectorAll('.drawer-close')) {
  button.addEventListener('click', () => closeDrawer(button.closest('.side-drawer')));
}
document.addEventListener('keydown', (event) => {
  if ($('outputDialog').open || $('catalogDialog').open || event.key !== 'Escape' || event.defaultPrevented) return;
  const drawer = document.querySelector('.side-drawer[open]');
  if (drawer) {
    event.preventDefault();
    closeDrawer(drawer);
  }
});
work(async () => {
  const { listing } = await request('/api/example');
  $('listing').value = listing;
  await createSession(listing);
});





