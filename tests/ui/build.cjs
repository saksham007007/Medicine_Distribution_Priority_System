// Bundles the React app with esbuild using the test shims, into tests/ui/build (served by the C++ server's --static option).
const path = require('path'), fs = require('fs');
const esbuild = require('esbuild');
const root = path.resolve(__dirname, '../..'), out = path.join(__dirname, 'build');
fs.mkdirSync(out, { recursive: true });
// lucide-react shim: one component per icon name actually imported by the app
const names = new Set();
(function scan(dir) { for (const f of fs.readdirSync(dir, { withFileTypes: true })) { const p = path.join(dir, f.name); if (f.isDirectory()) scan(p); else if (/\.jsx?$/.test(f.name)) {
  for (const m of fs.readFileSync(p, 'utf8').matchAll(/import\s*\{([^}]*)\}\s*from\s*'lucide-react'/g)) m[1].split(',').map((s) => s.trim().split(/\s+as\s+/)[0]).filter(Boolean).forEach((n) => names.add(n)); } } })(path.join(root, 'frontend/src'));
fs.writeFileSync(path.join(out, 'lucide-shim.jsx'), "import React from 'react';\n" + [...names].map((n) => `export const ${n} = ({ size = 16, className }) => <svg data-icon="${n}" width={size} height={size} className={className} />;`).join('\n') + '\n');
esbuild.buildSync({
  entryPoints: [path.join(root, 'frontend/src/main.jsx')], bundle: true, format: 'iife', outfile: path.join(out, 'app.js'), loader: { '.js': 'jsx', '.jsx': 'jsx', '.css': 'empty' },
  alias: { 'react-router-dom': path.join(__dirname, 'shims/react-router-dom.jsx'), recharts: path.join(__dirname, 'shims/recharts.jsx'), 'lucide-react': path.join(out, 'lucide-shim.jsx') },
  nodePaths: ['/opt/npm-tools/node_modules'], define: { 'process.env.NODE_ENV': '"development"', 'import.meta.env': '{}' }, logLevel: 'warning',
});
fs.writeFileSync(path.join(out, 'index.html'), '<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>MDPS test build</title></head><body><div id="root"></div><script src="/app.js"></script></body></html>');
console.log('bundled app.js (' + Math.round(fs.statSync(path.join(out, 'app.js')).size / 1024) + ' KB), icons: ' + names.size);
