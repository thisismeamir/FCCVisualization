# ROOT experiments (T4.3, T-B1-0)

Environment: ROOT 6.38.06 built from source in the development container
(features `opengl x11 webgui root7 http geom gdml imt`), GCC 13.3, Ubuntu 24.04,
Xvfb 21 with Mesa llvmpipe for an X display; no web browser. Sources of the
experiment programs: `docs/design/root-experiments/` (build with
`g++ -std=c++20 X.cc $(root-config --cflags --libs) -lEve -lRGL -lGed -lGui -lGeom -lEG`
or `-lROOTEve -lROOTWebDisplay -lRHTTP -lGeom` for REve).

| # | Question | Classic TEve | REve (`ROOT::Experimental`) |
|---|---|---|---|
| 1 | Runs in the target environment with a display; event loop driven by `gSystem->ProcessEvents()` instead of `Run()` | **Yes** under Xvfb (`teve1.cc`): `TApplication` + `TEveManager::Create`, points drawn, loop pumped by `ProcessEvents` | n/a (no display needed) |
| 2 | Serves with no `DISPLAY`; port/browser flow | n/a: `TEveManager::Create` needs X | **Yes** (`reve1.cc`): `Show(RWebDisplayArgs("server"))` starts THttpServer on 127.0.0.1 (random port), prints the window URL with key; `curl` gets HTTP 200. Over SSH: forward the port (`ssh -L <port>:localhost:<port>`) and open the printed URL locally |
| 3 | Pick and camera events deliverable to a callback | **Picks: yes** by polling `gEve->GetSelection()` after each `ProcessEvents` (`teve2.cc`: `UserPickedElement` shows up as a selected child). Signals (`TEveSelection::SelectionAdded`, `TGLViewer::Clicked`) need a slot class with a dictionary (`TQObject::Connect` by class name), so polling is used. **Camera: no signal**; a change of `TGLCamera::GetCamTrans()` is detectable by polling (`teve2.cc`) | **Not verified**: needs a browser client to produce picks and camera moves |
| 4 | Closing the main window | `TEveBrowser::CloseWindow()` calls `TEveManager::Terminate()`, which deletes `gEve`; the process keeps running (`teve2.cc`; ROOT 6.38 differs from the "terminates the app" in ARCHITECTURE). Detectable as `gEve == nullptr` → report `Port::Stopped` | **Not verified** (needs a browser) |
| 5 | Two backend instances | `TEveManager::Create` returns the existing `gEve` (`teve1.cc`): one process-wide manager; instances must share it (one runtime, as ARCHITECTURE decision 11 requires) | single `REveManager` too (`REveManager::Create` is static) |
| 6 | Offscreen image | **Yes** under Xvfb: `TGLViewer::SavePicture` and `SavePictureUsingFBO(file, w, h)` both write PNGs (`teve1.cc`) | not tested (needs a browser for screenshots) |

Other findings:

- Destroying `TApplication` after Eve was created crashes in `TGClient`
  cleanup (BadWindow, segfault in `~TGedEditor`), even after
  `TEveManager::Terminate()`. The backend therefore keeps one process-wide
  `TApplication` alive until the process ends and never deletes it.
- All TEve calls must stay on one thread (the backend's executor), which also
  pumps the event loop.

## Decision

The decision rule (SPEC_BACKENDS B1) requires REve to pass experiments 2–4 on
the target machine. REve passes 2; 3 and 4 cannot be verified here because
they need a browser. **Classic TEve is implemented first** (T4.4), behind an
internal viewer interface; REve is left for T4.5. To revisit on a machine with
a browser: run `reve1.cc`, open the URL, and check picks/camera/close
callbacks.

## Follow-up with a headless browser (T4.5)

Playwright's Chromium (headless, WebGL through SwiftShader) became available
after the decision, so experiments 3, 4 and 6 were repeated for REve with
`reve2.cc` and the driver `reve2_drive.mjs`:

| # | REve result |
|---|---|
| 3 | **Picks: yes.** A click on a point reaches the server as `NewElementPickedStr(id, multi, secondary, "indices")`; an `REveSelection::Deviator` receives the element and, with `SetAlwaysSecSelect(true)` on the point set, the index of the point. **Camera: no.** Rotating and zooming happen in the browser (three.js / RenderCore) and are not sent to the server. |
| 4 | **Closing the page: yes**, detectable as `RWebWindow::NumConnections()` dropping to 0 (polled after each event-loop pump); a reload reconnects. |
| 6 | Screenshots: only from the browser side (Playwright), not from the server. |

The browser needs the page's locale to be a valid BCP-47 tag (with
`LANG=C` Chromium reports `en-US@posix` and OpenUI5 fails to start; the
driver sets `en-US`), and REve's server URL must be the full one printed by
`Show` (with its key) or `RWebWindow::GetUrl(true)`.

With these results REve passes experiments 2–4 (camera events excepted), so
under the decision rule it would have qualified as well. Both viewers are
now implemented: classic TEve as `root` (T4.4) and REve as `reve` (T4.5),
sharing one ROOT thread.
