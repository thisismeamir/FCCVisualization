// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// Loads the Phoenix backend's output (events.json + OBJ files in <dir>) into the
// Phoenix event display in headless Chromium and compares what Phoenix built
// with what the files contain.
//   node verify.mjs <phoenix output dir> [screenshot.png]
//   node verify.mjs <phoenix output dir> [screenshot.png] --served http://127.0.0.1:<port>/
// With --served the page fetches the files from the backend's HTTP host (another
// origin, so CORS is exercised); the directory is still read for the expected values.
import { build } from 'esbuild';
import { chromium } from 'playwright';
import fs from 'node:fs';
import http from 'node:http';
import path from 'node:path';

const here = path.dirname(new URL(import.meta.url).pathname);
const args = process.argv.slice(2);
const servedAt = args.indexOf('--served');
const served = servedAt >= 0 ? args.splice(servedAt, 2)[1] : null;
const dataDir = path.resolve(args[0] ?? '.');
const shot = args[1];

await build({ entryPoints: [path.join(here, 'main.js')], bundle: true, outfile: path.join(here, 'bundle.js'),
              format: 'iife', logLevel: 'error', loader: { '.ttf': 'empty', '.woff': 'empty', '.woff2': 'empty' } });

const events = JSON.parse(fs.readFileSync(path.join(dataDir, 'events.json'), 'utf8'));
const objs = fs.readdirSync(dataDir).filter((f) => f.endsWith('.obj'));
const types = { '.html': 'text/html', '.js': 'text/javascript', '.json': 'application/json', '.obj': 'text/plain', '.mtl': 'text/plain' };
const server = http.createServer((req, res) => {
  const url = decodeURIComponent(req.url.split('?')[0]);
  if (url === '/objs.json') { res.writeHead(200, { 'content-type': 'application/json' }); return res.end(JSON.stringify(objs)); }
  const file = url.startsWith('/data/') ? path.join(dataDir, url.slice(6)) : path.join(here, url === '/' ? 'index.html' : url.slice(1));
  if (!fs.existsSync(file)) { res.writeHead(404); return res.end(); }
  res.writeHead(200, { 'content-type': types[path.extname(file)] ?? 'application/octet-stream' });
  fs.createReadStream(file).pipe(res);
}).listen(0, '127.0.0.1');
await new Promise((r) => server.once('listening', r));
const port = server.address().port;

const browser = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
const page = await browser.newPage({ viewport: { width: 900, height: 700 } });
const pageErrors = [];
page.on('pageerror', (e) => pageErrors.push(String(e)));
await page.goto(`http://127.0.0.1:${port}/` + (served ? '?base=' + encodeURIComponent(served) : ''));
await page.waitForFunction(() => window.fccvisCheck !== undefined, null, { timeout: 60000 });
await page.waitForTimeout(1500);
const result = await page.evaluate(() => window.fccvisCheck);
if (shot) await page.screenshot({ path: shot });
await browser.close();
server.close();

// Expected: every collection of the first event with its object count; one mesh per OBJ object.
const failures = [];
if (result.fatal) failures.push('page failed: ' + result.fatal);
const first = Object.keys(events)[0];
const expected = {};
for (const [type, cols] of Object.entries(events[first])) {
  if (cols && typeof cols === 'object' && !Array.isArray(cols)) for (const [n, items] of Object.entries(cols)) expected[type + '/' + n] = items.length;
}
const objObjects = objs.reduce((n, f) => n + fs.readFileSync(path.join(dataDir, f), 'utf8').split('\n').filter((l) => l.startsWith('o ')).length, 0);
if (!result.fatal) {
  if (JSON.stringify(result.keys) !== JSON.stringify(Object.keys(events))) failures.push(`event keys ${result.keys} != ${Object.keys(events)}`);
  for (const [k, n] of Object.entries(expected)) if (result.counts[k] !== n) failures.push(`${k}: Phoenix has ${result.counts[k]}, file has ${n}`);
  for (const k of Object.keys(result.counts)) if (!(k in expected)) failures.push(`unexpected collection ${k}`);
  if (result.meshes < objObjects) failures.push(`meshes: Phoenix has ${result.meshes}, OBJ files have ${objObjects} objects`);
  failures.push(...result.errors.map((e) => 'page error: ' + e));
}
failures.push(...pageErrors.map((e) => 'page error: ' + e));
console.log(JSON.stringify({ phoenix: '4.2.3', source: served ?? 'files', event: result.current, collections: result.counts, meshes: result.meshes }, null, 1));
if (failures.length) { console.log('FAIL\n  ' + failures.join('\n  ')); process.exit(1); }
console.log('ok: Phoenix loaded every collection and object');
