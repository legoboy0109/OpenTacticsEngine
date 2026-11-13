// Tiny static file server (Node http only). Serves a folder; files without an
// extension (the mock editor API's "files" list) are served as JSON.
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';

const TYPES = {
  '.html': 'text/html; charset=utf-8', '.js': 'text/javascript', '.mjs': 'text/javascript',
  '.css': 'text/css', '.json': 'application/json', '.wasm': 'application/wasm',
  '.ttf': 'font/ttf', '.png': 'image/png', '.txt': 'text/plain',
};

// Returns { url, port, close() }. `overrides` maps a URL path to {status, body}
// for tests that need a failing endpoint.
export function startServer(rootDir, overrides = {}) {
  const root = path.resolve(rootDir);
  const server = http.createServer((req, res) => {
    const urlPath = decodeURIComponent(new URL(req.url, 'http://x').pathname);
    const ov = overrides[urlPath];
    if (ov) { res.writeHead(ov.status, { 'content-type': 'text/plain' }); res.end(ov.body || ''); return; }
    const file = path.join(root, path.normalize(urlPath));
    if (!file.startsWith(root)) { res.writeHead(403); res.end(); return; }
    fs.stat(file, (err, st) => {
      if (err || !st.isFile()) { res.writeHead(404, { 'content-type': 'text/plain' }); res.end('not found: ' + urlPath); return; }
      const ext = path.extname(file).toLowerCase();
      res.writeHead(200, {
        'content-type': TYPES[ext] || (ext === '' ? 'application/json' : 'application/octet-stream'),
        'cache-control': 'no-store',
      });
      fs.createReadStream(file).pipe(res);
    });
  });
  return new Promise((resolve) => {
    server.listen(0, '127.0.0.1', () => {
      const port = server.address().port;
      resolve({ port, url: `http://127.0.0.1:${port}`, close: () => new Promise((r) => server.close(r)) });
    });
  });
}
