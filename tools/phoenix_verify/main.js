// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// Page script: a bare Phoenix event display that loads events.json and every
// OBJ listed in objs.json, from ./data/ or from ?base=<url>, then publishes what it built on
// window.fccvisCheck for verify.mjs.
import { EventDisplay } from 'phoenix-event-display';

async function run() {
  const errors = [];
  window.addEventListener('error', (e) => errors.push(String(e.message)));
  const display = new EventDisplay();
  display.init({ elementId: 'eventDisplay', defaultView: [4000, 4000, 4000, 0, 0, 0] });
  // ?base=<url> loads the files from another origin (the backend's own HTTP host).
  const base = new URLSearchParams(location.search).get('base') ?? 'data/';
  const events = await (await fetch(base + 'events.json')).json();
  const keys = display.parsePhoenixEvents(events);
  const objs = await (await fetch('objs.json')).json();
  for (const name of objs) {
    await display.loadOBJGeometry(base + name, name, 0x8d8d8d, undefined, true, true);
  }
  const collections = display.getCollections();
  const counts = {};
  for (const [type, names] of Object.entries(collections)) {
    for (const n of names) counts[type + '/' + n] = display.getCollection(n).length;
  }
  const geometry = display.getThreeManager().getSceneManager().getGeometries();
  let meshes = 0;
  geometry.traverse((o) => { if (o.isMesh) meshes += 1; });
  window.fccvisCheck = { keys, current: display.getCurrentEventKey(), counts, meshes, errors };
}
run().catch((e) => { window.fccvisCheck = { fatal: String(e && e.stack || e) }; });
