// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
import { chromium } from 'playwright';  // run with tools/phoenix_verify/node_modules resolvable (NODE_PATH does not apply to ESM: copy next to it)
import fs from 'node:fs';
const url = fs.readFileSync('url.txt', 'utf8').trim();
const browser = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
const page = await browser.newPage({ viewport: { width: 1000, height: 700 }, locale: 'en-US' });
page.on('console', (m) => { if (m.type() === 'error') console.log('console:', m.text()); });
page.on('pageerror', (e) => console.log('pageerror:', String(e)));
await page.goto(url);
await page.waitForTimeout(8000);
await page.screenshot({ path: 'reve.png' });
const canvases = await page.$$eval('canvas', (cs) => cs.map((c) => [c.width, c.height, c.getBoundingClientRect().x, c.getBoundingClientRect().y]));
console.log('canvases', JSON.stringify(canvases));
const big = canvases.sort((a, b) => b[0] * b[1] - a[0] * a[1])[0];
if (big) {
  const cx = big[2] + big[0] / 2, cy = big[3] + big[1] / 2;
  await page.mouse.click(cx, cy); await page.waitForTimeout(1500);
  await page.mouse.click(cx + 3, cy + 2); await page.waitForTimeout(1500);
  // drag to rotate
  await page.mouse.move(cx, cy); await page.mouse.down(); await page.mouse.move(cx + 120, cy + 40, { steps: 10 }); await page.mouse.up();
  await page.waitForTimeout(1500);
  await page.screenshot({ path: 'reve2.png' });
}
await page.close(); await browser.close();
console.log('closed');
