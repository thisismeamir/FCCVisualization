// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// Drives the REve page of the "reve" backend in headless Chromium (test_reve.cc):
// waits for the viewers, clicks the centre of the first one, takes a screenshot
// and closes the page. Usage: node reve_drive.mjs <url> <screenshot.png>
// Playwright is loaded from $FCCVIS_NODE_MODULES (tools/phoenix_verify/node_modules).
import { createRequire } from 'node:module';
const require = createRequire((process.env.FCCVIS_NODE_MODULES ?? process.cwd()) + '/');
const { chromium } = require('playwright');

const [url, shot] = process.argv.slice(2);
const browser = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
const page = await browser.newPage({ viewport: { width: 1000, height: 700 }, locale: 'en-US' });
const errors = [];
page.on('pageerror', (e) => errors.push(String(e)));
await page.goto(url);
await page.waitForFunction(() => document.querySelectorAll('canvas').length > 0, null, { timeout: 60000 });
await page.waitForTimeout(4000);
const canvases = await page.$$eval('canvas', (cs) => cs.map((c) => {
  const r = c.getBoundingClientRect();
  return [r.x, r.y, r.width, r.height];
}));
const [x, y, w, h] = canvases.sort((a, b) => b[2] * b[3] - a[2] * a[3])[0];
await page.mouse.click(x + w / 2, y + h / 2);
await page.waitForTimeout(2000);
if (shot) await page.screenshot({ path: shot });
await page.close();
await browser.close();
console.log(JSON.stringify({ canvases: canvases.length, errors }));
