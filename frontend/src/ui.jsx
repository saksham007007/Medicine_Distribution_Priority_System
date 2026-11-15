// Shared UI building blocks: data hook, toast, badges, tables, modal, form fields, async pickers.
import React, { useCallback, useEffect, useRef, useState, createContext, useContext } from 'react';
import { X, Search } from 'lucide-react';

// ---------- data loading ----------
export function useAsync(fn, deps = []) {
  const [state, setState] = useState({ data: null, loading: true, error: null });
  const seq = useRef(0);
  const run = useCallback(() => {
    const id = ++seq.current;
    setState((s) => ({ ...s, loading: true, error: null }));
    return fn()
      .then((data) => { if (id === seq.current) setState({ data, loading: false, error: null }); })
      .catch((e) => { if (id === seq.current) setState({ data: null, loading: false, error: e.message || String(e) }); });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, deps);
  useEffect(() => { run(); }, [run]);
  return { ...state, reload: run };
}

export function useDebounced(value, ms = 300) {
  const [v, setV] = useState(value);
  useEffect(() => { const t = setTimeout(() => setV(value), ms); return () => clearTimeout(t); }, [value, ms]);
  return v;
}

// ---------- toast ----------
const ToastContext = createContext({ success() {}, error() {} });
export const useToast = () => useContext(ToastContext);
export function ToastProvider({ children }) {
  const [items, setItems] = useState([]);
  const push = useCallback((kind, text) => {
    const id = Math.random();
    setItems((x) => [...x, { id, kind, text }]);
    setTimeout(() => setItems((x) => x.filter((i) => i.id !== id)), 5000);
  }, []);
  const api = { success: (t) => push('success', t), error: (t) => push('error', t) };
  return (
    <ToastContext.Provider value={api}>
      {children}
      <div className="fixed bottom-4 right-4 z-50 flex max-w-sm flex-col gap-2" role="status" aria-live="polite" data-testid="toasts">
        {items.map((i) => (
          <div key={i.id} data-kind={i.kind} className={'rounded-lg px-4 py-3 text-sm text-white shadow-lg ' + (i.kind === 'error' ? 'bg-red-600' : 'bg-teal-600')}>{i.text}</div>
        ))}
      </div>
    </ToastContext.Provider>
  );
}

// ---------- presentation ----------
export const fmtDate = (s) => (s ? String(s).replace('T', ' ').slice(0, 16) : '');
export const money = (n) => (n === null || n === undefined ? '' : '₹' + Number(n).toFixed(2));
export const minutes = (m) => (m === null || m === undefined ? '' : m < 60 ? `${m} min` : m < 1440 ? `${Math.floor(m / 60)}h ${m % 60}m` : `${Math.floor(m / 1440)}d ${Math.floor((m % 1440) / 60)}h`);

const BADGE = {
  Critical: 'bg-red-100 text-red-700', High: 'bg-orange-100 text-orange-700', Medium: 'bg-amber-100 text-amber-700', Normal: 'bg-slate-100 text-slate-600',
  Pending: 'bg-blue-100 text-blue-700', Waiting: 'bg-amber-100 text-amber-700', Distributed: 'bg-teal-100 text-teal-700', Rejected: 'bg-red-100 text-red-700',
  Cancelled: 'bg-slate-100 text-slate-500', Completed: 'bg-teal-100 text-teal-700', Open: 'bg-teal-100 text-teal-700', Closed: 'bg-slate-100 text-slate-600',
  Suspended: 'bg-red-100 text-red-700', 'Low stock': 'bg-amber-100 text-amber-700', 'Out of stock': 'bg-red-100 text-red-700', OK: 'bg-teal-100 text-teal-700',
  Expired: 'bg-red-100 text-red-700', 'Near expiry': 'bg-amber-100 text-amber-700', Active: 'bg-teal-100 text-teal-700', Discontinued: 'bg-slate-100 text-slate-500',
  Admin: 'bg-purple-100 text-purple-700', Pharmacist: 'bg-blue-100 text-blue-700', Patient: 'bg-teal-100 text-teal-700',
  Proposed: 'bg-amber-100 text-amber-700', Approved: 'bg-teal-100 text-teal-700', None: 'bg-slate-100 text-slate-500',
};
export function Badge({ value, children }) {
  return <span className={'inline-block whitespace-nowrap rounded-full px-2 py-0.5 text-xs font-semibold ' + (BADGE[value] || 'bg-slate-100 text-slate-600')}>{children || value}</span>;
}

export function Spinner({ label = 'Loading…' }) {
  return <div className="flex items-center gap-2 p-6 text-sm text-slate-500" role="status"><span className="h-4 w-4 animate-spin rounded-full border-2 border-brand-500 border-t-transparent" />{label}</div>;
}
export function ErrorBox({ error, onRetry }) {
  if (!error) return null;
  return (
    <div className="flex items-center justify-between gap-3 rounded-lg border border-red-200 bg-red-50 p-3 text-sm text-red-700" role="alert">
      <span>{error}</span>{onRetry && <button className="btn-ghost btn-sm" onClick={onRetry}>Retry</button>}
    </div>
  );
}
export function PageHeader({ title, subtitle, children }) {
  return (
    <div className="mb-4 flex flex-wrap items-start justify-between gap-3">
      <div><h1 className="text-xl font-bold text-slate-900" data-testid="page-title">{title}</h1>{subtitle && <p className="text-sm text-slate-500">{subtitle}</p>}</div>
      <div className="flex flex-wrap items-center gap-2">{children}</div>
    </div>
  );
}
export function StatCard({ label, value, tone = 'brand', hint }) {
  const tones = { brand: 'text-brand-600', teal: 'text-teal-600', red: 'text-red-600', amber: 'text-amber-600' };
  return (
    <div className="card" data-testid={'stat-' + label.toLowerCase().replace(/[^a-z0-9]+/g, '-')}>
      <div className="text-xs font-semibold uppercase tracking-wide text-slate-500">{label}</div>
      <div className={'mt-1 text-2xl font-bold ' + tones[tone]}>{value ?? '–'}</div>
      {hint && <div className="mt-1 text-xs text-slate-400">{hint}</div>}
    </div>
  );
}

export function DataTable({ columns, rows, empty = 'No records found.', rowKey, onRowClick }) {
  if (!rows || rows.length === 0) return <div className="rounded-lg border border-dashed border-slate-300 p-6 text-center text-sm text-slate-500">{empty}</div>;
  return (
    <div className="overflow-x-auto rounded-xl border border-slate-200 bg-white">
      <table className="min-w-full divide-y divide-slate-100">
        <thead><tr>{columns.map((c) => <th key={c.key || c.label} className="th">{c.label}</th>)}</tr></thead>
        <tbody className="divide-y divide-slate-100">
          {rows.map((r, i) => (
            <tr key={rowKey ? r[rowKey] : i} className={onRowClick ? 'cursor-pointer hover:bg-slate-50' : ''} onClick={onRowClick ? () => onRowClick(r) : undefined}>
              {columns.map((c) => <td key={c.key || c.label} className="td">{c.render ? c.render(r) : r[c.key]}</td>)}
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

export function Pagination({ total, limit, offset, onChange }) {
  if (!total || total <= limit) return total ? <div className="mt-2 text-xs text-slate-500">{total} record(s)</div> : null;
  const page = Math.floor(offset / limit) + 1, pages = Math.ceil(total / limit);
  return (
    <div className="mt-3 flex items-center justify-between text-sm">
      <span className="text-slate-500">{total} records · page {page} of {pages}</span>
      <div className="flex gap-2">
        <button className="btn-ghost btn-sm" disabled={offset <= 0} onClick={() => onChange(Math.max(0, offset - limit))}>Previous</button>
        <button className="btn-ghost btn-sm" disabled={offset + limit >= total} onClick={() => onChange(offset + limit)}>Next</button>
      </div>
    </div>
  );
}

export function Tabs({ tabs, value, onChange }) {
  return (
    <div className="mb-4 flex flex-wrap gap-1 border-b border-slate-200" role="tablist">
      {tabs.map((t) => (
        <button key={t.value} role="tab" aria-selected={value === t.value} onClick={() => onChange(t.value)}
          className={'-mb-px border-b-2 px-3 py-2 text-sm font-medium ' + (value === t.value ? 'border-brand-500 text-brand-600' : 'border-transparent text-slate-500 hover:text-slate-700')}>{t.label}</button>
      ))}
    </div>
  );
}

export function Modal({ title, onClose, children, wide }) {
  useEffect(() => { const h = (e) => e.key === 'Escape' && onClose(); window.addEventListener('keydown', h); return () => window.removeEventListener('keydown', h); }, [onClose]);
  return (
    <div className="fixed inset-0 z-40 flex items-start justify-center overflow-y-auto bg-slate-900/50 p-4">
      <div role="dialog" aria-modal="true" aria-label={title} className={'mt-8 w-full rounded-xl bg-white shadow-xl ' + (wide ? 'max-w-4xl' : 'max-w-lg')}>
        <div className="flex items-center justify-between border-b border-slate-200 px-4 py-3">
          <h2 className="font-semibold text-slate-900">{title}</h2>
          <button onClick={onClose} aria-label="Close" className="rounded p-1 text-slate-500 hover:bg-slate-100"><X size={18} /></button>
        </div>
        <div className="p-4">{children}</div>
      </div>
    </div>
  );
}

// ---------- form fields ----------
export function Field({ label, hint, children }) {
  return <label className="block"><span className="label">{label}</span>{children}{hint && <span className="mt-1 block text-xs text-slate-400">{hint}</span>}</label>;
}
export function Input({ label, hint, ...props }) { return <Field label={label} hint={hint}><input className="input" {...props} /></Field>; }
export function Textarea({ label, hint, ...props }) { return <Field label={label} hint={hint}><textarea className="input" rows={3} {...props} /></Field>; }
export function Select({ label, hint, options, placeholder, ...props }) {
  return (
    <Field label={label} hint={hint}>
      <select className="input" {...props}>
        {placeholder !== undefined && <option value="">{placeholder}</option>}
        {options.map((o) => { const v = typeof o === 'object' ? o.value : o; const l = typeof o === 'object' ? o.label : o; return <option key={v} value={v}>{l}</option>; })}
      </select>
    </Field>
  );
}

// Debounced async search box that lets the user pick one item (patients, medicines, ...).
export function SearchSelect({ label, fetcher, renderItem, picked, onPick, placeholder = 'Type to search…' }) {
  const [q, setQ] = useState('');
  const [items, setItems] = useState([]);
  const [open, setOpen] = useState(false);
  const dq = useDebounced(q, 250);
  useEffect(() => {
    if (!open) return undefined;
    let live = true;
    fetcher(dq).then((r) => live && setItems(r)).catch(() => live && setItems([]));
    return () => { live = false; };
  }, [dq, open]); // eslint-disable-line react-hooks/exhaustive-deps
  if (picked) {
    return (
      <div><span className="label">{label}</span>
        <div className="flex items-center justify-between rounded-lg border border-teal-300 bg-teal-50 px-3 py-2 text-sm" data-testid="picked">
          <span>{renderItem(picked)}</span><button type="button" className="text-xs text-slate-500 underline" onClick={() => { onPick(null); setQ(''); }}>change</button>
        </div></div>
    );
  }
  return (
    <div className="relative"><span className="label">{label}</span>
      <div className="relative"><Search size={14} className="absolute left-3 top-3 text-slate-400" />
        <input className="input pl-8" value={q} placeholder={placeholder} onFocus={() => setOpen(true)} onChange={(e) => { setQ(e.target.value); setOpen(true); }} aria-label={label} /></div>
      {open && (
        <ul className="absolute z-30 mt-1 max-h-60 w-full overflow-auto rounded-lg border border-slate-200 bg-white shadow-lg" role="listbox">
          {items.length === 0 && <li className="px-3 py-2 text-sm text-slate-400">No matches</li>}
          {items.map((it, i) => (
            <li key={i}><button type="button" role="option" className="block w-full px-3 py-2 text-left text-sm hover:bg-brand-50" onClick={() => { onPick(it); setOpen(false); }}>{renderItem(it)}</button></li>
          ))}
        </ul>
      )}
    </div>
  );
}
