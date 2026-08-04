# Connect Four Spatial Lab

A local React/Three.js interface backed by the repository's native C++23 AIs.

## Run locally

```bash
cd gui
npm install
npm run dev
```

Open `http://127.0.0.1:5173`. The dev command compiles `native/bridge.cpp` before starting the server. Production mode is available with `npm start`.

## Verify

```bash
npm run check
```

`npm run check` compiles and tests the native bridge, runs the API and presentation tests, builds the production client, and executes the Playwright browser smoke test. On a new machine, install its browser once with `npx playwright install --with-deps chromium`.

Runtime archives are written to `gui/data/` as private, schema-versioned JSON files and are ignored by git. The HTTP server binds only to `127.0.0.1`.
