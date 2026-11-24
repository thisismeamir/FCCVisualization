# Phoenix check

Loads the output of the Phoenix backend (`events.json` and the OBJ files) into
the Phoenix event display (`phoenix-event-display` from npm, the library the
Phoenix web application is built on) in headless Chromium, and compares what
Phoenix built with the files: every event key, every collection with its
number of objects, one mesh per OBJ object, and no page errors.

```sh
cd tools/phoenix_verify
npm install                      # phoenix-event-display 4.2.3, playwright, esbuild
node verify.mjs <output_dir> [screenshot.png]
node verify.mjs <output_dir> [screenshot.png] --served http://127.0.0.1:<port>/
```

The second form fetches the files from the backend's own HTTP host (started
with `port=<port>`), from another origin, so the CORS headers are exercised.

Playwright needs a Chromium build (`npx playwright install chromium`, or set
`PLAYWRIGHT_BROWSERS_PATH`); WebGL runs on SwiftShader. When `node` and
`node_modules` are present at configure time, CTest runs this check as
`phoenix::viewer` (label `needs-node`).
