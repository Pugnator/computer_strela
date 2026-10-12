import http from 'node:http';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const dist = path.join(path.dirname(fileURLToPath(import.meta.url)), 'dist');
const host = '127.0.0.1';
const port = Number(process.env.STRELA_PORT || 4318);
const types = {
  '.html': 'text/html; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.wasm': 'application/wasm',
  '.jpg': 'image/jpeg',
  '.strela': 'text/plain; charset=utf-8',
  '.json': 'application/json; charset=utf-8'
};
const files = new Set([
  'index.html', 'style.css', 'app.js', 'wasm-api.mjs', 'upp.mjs',
  'strela_wasm.mjs', 'strela_wasm.wasm', 'kitov-linear.strela', 'vostok-orbit.strela', 'catalog.json',
  'strela-control-panel-polymus.jpg', 'strela-lamp-panel-polymus.jpg'
]);
const server = http.createServer(async (request, response) => {
  try {
    const name = new URL(request.url, 'http://localhost').pathname.slice(1) || 'index.html';
    const program = /^programs\/[a-z0-9-]+\.strela$/.test(name);
    if ((!files.has(name) && !program) || !['GET', 'HEAD'].includes(request.method)) {
      response.writeHead(404);
      response.end();
      return;
    }
    const bytes = await readFile(path.join(dist, name));
    response.writeHead(200, {
      'Content-Type': types[path.extname(name)],
      'Content-Length': bytes.length
    });
    response.end(request.method === 'HEAD' ? undefined : bytes);
  } catch {
    response.writeHead(404);
    response.end();
  }
});
server.listen(port, host, () =>
  console.log('Static Strela playground: http://' + host + ':' + server.address().port));

