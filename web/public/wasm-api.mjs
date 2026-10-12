const oct4 = (s, max = 0o3777) =>
  typeof s === 'string' && /^[0-7]{4}$/.test(s) && parseInt(s, 8) <= max;
const asOct = (n) => n.toString(8).padStart(4, '0');

function parseListing(text) {
  if (typeof text !== 'string' || text.length > 1024 * 1024 || !text.trim() ||
      new TextEncoder().encode(text).length > 1024 * 1024)
    throw new Error('Provide an octal listing up to 1 MB');
  const seen = new Set();
  const words = [];
  let start = '0000';
  let hasStart = false;
  let lineNumber = 0;
  for (const line of text.split(/\r?\n/)) {
    lineNumber++;
    const trimmed = line.trim();
    if (!trimmed || trimmed.startsWith('#')) continue;
    const t = trimmed.split(/\s+/);
    if (t[0] === 'START') {
      if (hasStart || t.length !== 2 || !oct4(t[1]))
        throw new Error('Invalid START on line ' + lineNumber);
      start = t[1];
      hasStart = true;
      continue;
    }
    if (t.length !== 6 || !oct4(t[0]) ||
        !t.slice(1, 4).every((value) => oct4(value, 0o7777)) ||
        !/^[01]$/.test(t[4]) || !/^[0-7]{2}$/.test(t[5]) || seen.has(t[0]))
      throw new Error('Invalid or duplicate octal listing on line ' + lineNumber);
    seen.add(t[0]);
    const a = BigInt(parseInt(t[1], 8));
    const b = BigInt(parseInt(t[2], 8));
    const c = BigInt(parseInt(t[3], 8));
    const check = BigInt(Number(t[4]));
    const opcode = BigInt(parseInt(t[5], 8));
    const packed = (a << 31n) | (b << 19n) | (c << 7n) | (check << 6n) | opcode;
    words.push([parseInt(t[0], 8), packed.toString(8).padStart(15, '0')]);
  }
  return { start, words };
}

function nativeCalls(module) {
  const bind = (name, result, args) => module.cwrap(name, result, args);
  return {
    create: bind('sb_new', 'number', []),
    destroy: bind('sb_destroy', null, []),
    write: bind('sb_write_word', 'number', ['number', 'string']),
    setPc: bind('sb_set_pc', 'number', ['number']),
    mountCards: bind('sb_mount_cards', 'number', ['string']),
    mountTape: bind('sb_mount_tape', 'number', ['number', 'string']),
    pause: bind('sb_pause', 'number', []),
    reset: bind('sb_reset', 'number', ['number']),
    check: bind('sb_check', 'number', ['number']),
    step: bind('sb_step', 'number', ['number']),
    run: bind('sb_run_us', 'number', ['string']),
    save: bind('sb_save_snapshot', 'number', ['string']),
    load: bind('sb_load_snapshot', 'number', ['string']),
    punched: bind('sb_punched_json', 'string', ['number', 'number']),
    state: bind('sb_state_json', 'string', ['number', 'number'])
  };
}

function requireSuccess(value, message) {
  if (!value) throw new Error(message);
}

export function createWasmBackend(createModule, readExample) {
  const sessions = new Map();
  const encoder = new TextEncoder();

  function current(id) {
    const session = sessions.get(id);
    if (!session) throw new Error('Session not found');
    return session;
  }

  function inspect(session, url) {
    if (url.searchParams.has('cards')) {
      const start = Number(url.searchParams.get('cards'));
      const limit = Number(url.searchParams.get('count') || '128');
      if (!Number.isSafeInteger(start) || start < 0 || start > 0xffffffff ||
          !Number.isInteger(limit) || limit < 1 || limit > 128)
        throw new Error('Invalid punched deck range');
      const data = session.native.punched(start, limit);
      if (!data) throw new Error('Cannot inspect punched deck');
      return JSON.parse(data);
    }
    const ram = url.searchParams.get('ram') || '0000';
    const zone = url.searchParams.get('zone');
    if (!oct4(ram) ||
        (zone !== null && !(oct4(zone, 0o5777) && /^[45][0-7]{3}$/.test(zone) && !/^[45]000$/.test(zone))))
      throw new Error('Invalid octal memory or tape address');
    const data = session.native.state(parseInt(ram, 8), zone ? parseInt(zone, 8) : 0);
    if (!data) throw new Error('Cannot inspect machine state');
    return JSON.parse(data);
  }

  async function makeModule() {
    const module = await createModule();
    const native = nativeCalls(module);
    requireSuccess(native.create(), 'Cannot create emulator');
    return { module, native };
  }

  return {
    async request(urlText, options = {}) {
      const url = new URL(urlText, 'http://strela.local');
      if (url.pathname === '/api/example') return { listing: await readExample() };
      if (url.pathname === '/api/session' && options.method === 'POST') {
        const input = JSON.parse(options.body);
        const parsed = parseListing(input.listing);
        if (input.start !== undefined) {
          if (!oct4(input.start)) throw new Error('Start address must be four octal digits');
          parsed.start = input.start;
        }
        let mediaBytes = 0;
        for (const key of ['cards', 'tape1', 'tape2']) {
          if (input[key] === undefined) continue;
          if (typeof input[key] !== 'string' || input[key].length > 8 * 1024 * 1024)
            throw new Error(key + ' image must be text under 8 MB');
          mediaBytes += encoder.encode(input[key]).length;
          if (mediaBytes > 16 * 1024 * 1024)
            throw new Error('Combined media image size must be under 16 MB');
        }
        const session = await makeModule();
        for (const [address, word] of parsed.words)
          requireSuccess(session.native.write(address, word), 'Cannot write program word');
        requireSuccess(session.native.setPc(parseInt(parsed.start, 8)), 'Invalid start address');
        for (const key of ['cards', 'tape1', 'tape2']) {
          if (input[key] === undefined) continue;
          if (typeof input[key] !== 'string' || encoder.encode(input[key]).length > 8 * 1024 * 1024)
            throw new Error(key + ' image must be text under 8 MB');
          const path = '/' + key + '.txt';
          session.module.FS.writeFile(path, encoder.encode(input[key]));
          requireSuccess(key === 'cards' ? session.native.mountCards(path) :
            session.native.mountTape(key === 'tape1' ? 0 : 1, path), 'Invalid ' + key + ' image');
          session.module.FS.unlink(path);
        }
        const id = crypto.randomUUID();
        sessions.set(id, session);
        return { id, state: inspect(session, url) };
      }
      if (url.pathname === '/api/session/import' && options.method === 'POST') {
        const input = options.body;
        if (!(input instanceof ArrayBuffer) && !ArrayBuffer.isView(input) ||
            input.byteLength > 16 * 1024 * 1024)
          throw new Error('Snapshot must be under 16 MB');
        const bytes = input instanceof ArrayBuffer ? new Uint8Array(input) :
          new Uint8Array(input.buffer, input.byteOffset, input.byteLength);
        const session = await makeModule();
        session.module.FS.writeFile('/import.sts', bytes);
        requireSuccess(session.native.load('/import.sts'), 'Invalid snapshot');
        session.module.FS.unlink('/import.sts');
        const id = crypto.randomUUID();
        sessions.set(id, session);
        return { id, state: inspect(session, url) };
      }
      const match = /^\/api\/session\/([0-9a-f-]{36})(?:\/(action|snapshot))?$/.exec(url.pathname);
      if (!match) throw new Error('Unknown browser emulator request');
      const session = current(match[1]);
      if (!match[2] && options.method === 'DELETE') {
        session.native.destroy();
        sessions.delete(match[1]);
        return { released: true };
      }
      if (!match[2] && (!options.method || options.method === 'GET'))
        return inspect(session, url);
      if (match[2] === 'action' && options.method === 'POST') {
        const action = JSON.parse(options.body);
        const n = session.native;
        switch (action.type) {
        case 'step':
          if (!Number.isInteger(action.count) || action.count < 1 || action.count > 10000)
            throw new Error('Step count must be 1 to 10000');
          requireSuccess(n.step(action.count), 'Cannot step machine'); break;
        case 'run':
          if (!Number.isSafeInteger(action.budgetUs) || action.budgetUs < 1 || action.budgetUs > 1000000000)
            throw new Error('Time budget must be 1 to 1000000000 microseconds');
          requireSuccess(n.run(String(action.budgetUs)), 'Cannot run machine'); break;
        case 'reset':
          if (!oct4(action.address)) throw new Error('Invalid reset address');
          requireSuccess(n.reset(parseInt(action.address, 8)), 'Cannot reset machine'); break;
        case 'set':
          if (!oct4(action.address, 0o7777) || typeof action.word !== 'string' ||
              !/^[0-7]{15}$/.test(action.word) || BigInt('0o' + action.word) > 0x7ffffffffffn)
            throw new Error('Invalid word edit');
          requireSuccess(n.pause(), 'Cannot pause machine');
          requireSuccess(n.write(parseInt(action.address, 8), action.word), 'Cannot write word');
          break;
        case 'check':
          if (typeof action.enabled !== 'boolean') throw new Error('Invalid check switch');
          requireSuccess(n.check(action.enabled ? 1 : 0), 'Cannot set check switch'); break;
        default: throw new Error('Unknown operator action');
        }
        return inspect(session, url);
      }
      throw new Error('Unknown browser emulator request');
    },
    exportSnapshot(id) {
      const session = current(id);
      requireSuccess(session.native.save('/export.sts'), 'Cannot save snapshot');
      const bytes = session.module.FS.readFile('/export.sts');
      session.module.FS.unlink('/export.sts');
      return bytes;
    }
  };
}

