TEST-ONLY stand-ins for react-router-dom, recharts and lucide-react.
The sandbox that produced this project has no network, so these three libraries could not be installed. The shims implement just the
API subset the app uses (hash routing, chart containers that expose their data, icons) so the application code can be bundled with
esbuild and exercised in a real headless Chromium against the real C++ server.
They are NOT used by the real build (npm install + vite build use the real libraries). See docs/TESTING.md for what this does and does not prove.
