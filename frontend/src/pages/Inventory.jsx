// VANSH - inventory: stock levels, batches, expiry, low-stock alerts, restocking, movements.
import React, { useState } from 'react';
import { Plus, RefreshCw, Pencil, Layers } from 'lucide-react';
import { api } from '../api.js';
import { useAuth } from '../auth.jsx';
import { useAsync, useDebounced, PageHeader, DataTable, Pagination, Badge, Modal, Input, Select, ErrorBox, Spinner, SearchSelect, Tabs, useToast, fmtDate } from '../ui.jsx';

const LIMIT = 15;
const iso = (d) => d.toISOString().slice(0, 10);
const plusDays = (n) => iso(new Date(Date.now() + n * 86400000));

function RestockForm({ pharmacies, preset, onClose, onDone }) {
  const { user } = useAuth(); const toast = useToast();
  const [pharmacy, setPharmacy] = useState(String(preset?.pharmacy_id || user.pharmacy_id || ''));
  const [medicine, setMedicine] = useState(preset ? { medicine_id: preset.medicine_id, name: preset.medicine } : null);
  const [f, setF] = useState({ quantity: 50, batch_number: '', manufacturing_date: plusDays(-30), expiry_date: plusDays(540) });
  const [error, setError] = useState(''), [busy, setBusy] = useState(false);
  const set = (k) => (e) => setF({ ...f, [k]: e.target.value });
  async function submit(e) {
    e.preventDefault(); setBusy(true); setError('');
    try {
      const r = await api.post('/api/inventory/restock', { pharmacy_id: Number(pharmacy), medicine_id: medicine.medicine_id, quantity: Number(f.quantity), batch_number: f.batch_number, manufacturing_date: f.manufacturing_date, expiry_date: f.expiry_date });
      toast.success(`Stock updated (now ${r.new_quantity}).` + (r.promoted_from_waiting_list ? ` ${r.promoted_from_waiting_list} waiting request(s) returned to the queue.` : '')); onDone();
    } catch (err) { setError(err.message); } finally { setBusy(false); }
  }
  return (
    <Modal title="Restock medicine" onClose={onClose}>
      <form onSubmit={submit} className="space-y-3">
        <Select label="Pharmacy" value={pharmacy} onChange={(e) => setPharmacy(e.target.value)} placeholder="Select pharmacy…" required options={pharmacies.map((p) => ({ value: p.pharmacy_id, label: `${p.name} (${p.city})` }))} />
        <SearchSelect label="Medicine" picked={medicine} onPick={setMedicine} fetcher={(q) => api.get('/api/medicines', { q, limit: 10 }).then((r) => r.items)} renderItem={(m) => m.name + (m.generic_name ? ` (${m.generic_name})` : '')} />
        <div className="grid grid-cols-2 gap-3"><Input label="Quantity" type="number" min="1" value={f.quantity} onChange={set('quantity')} required /><Input label="Batch number" value={f.batch_number} onChange={set('batch_number')} required minLength={2} /></div>
        <div className="grid grid-cols-2 gap-3"><Input label="Manufacturing date" type="date" max={iso(new Date())} value={f.manufacturing_date} onChange={set('manufacturing_date')} required /><Input label="Expiry date" type="date" min={plusDays(1)} value={f.expiry_date} onChange={set('expiry_date')} required /></div>
        <ErrorBox error={error} />
        <div className="flex justify-end gap-2"><button type="button" className="btn-ghost" onClick={onClose}>Cancel</button><button className="btn-teal" disabled={busy || !medicine || !pharmacy}>{busy ? 'Saving…' : 'Add stock'}</button></div>
      </form>
    </Modal>
  );
}

function ReorderForm({ row, onClose, onDone }) {
  const toast = useToast(); const [lvl, setLvl] = useState(row.reorder_level), [error, setError] = useState('');
  async function submit(e) { e.preventDefault(); try { await api.put(`/api/inventory/${row.pharmacy_id}/${row.medicine_id}`, { reorder_level: Number(lvl) }); toast.success('Reorder level updated'); onDone(); } catch (err) { setError(err.message); } }
  return (
    <Modal title={'Reorder level · ' + row.medicine} onClose={onClose}>
      <form onSubmit={submit} className="space-y-3"><p className="text-sm text-slate-500">{row.pharmacy}</p><Input label="Reorder level (units)" type="number" min="0" value={lvl} onChange={(e) => setLvl(e.target.value)} required />
        <ErrorBox error={error} /><div className="flex justify-end gap-2"><button type="button" className="btn-ghost" onClick={onClose}>Cancel</button><button className="btn-primary">Save</button></div></form>
    </Modal>
  );
}

function Batches({ row, onClose }) {
  const b = useAsync(() => api.get('/api/inventory/batches', { pharmacy_id: row.pharmacy_id, medicine_id: row.medicine_id }), [row.pharmacy_id, row.medicine_id]);
  return (
    <Modal title={`Batches · ${row.medicine}`} onClose={onClose} wide>
      <p className="mb-2 text-sm text-slate-500">{row.pharmacy}. Stock is dispensed First-Expiry-First-Out.</p>
      {b.loading && <Spinner />}<ErrorBox error={b.error} />
      {b.data && <DataTable rowKey="batch_id" rows={b.data.items} columns={[{ label: 'Batch', key: 'batch_number' }, { label: 'Manufactured', key: 'manufacturing_date' }, { label: 'Expiry', key: 'expiry_date' }, { label: 'Units', key: 'quantity' }, { label: 'Status', render: (x) => <Badge value={x.batch_status === 'OK' ? 'OK' : x.batch_status}>{x.batch_status}</Badge> }]} />}
    </Modal>
  );
}

export default function Inventory() {
  const { user } = useAuth(); const toast = useToast();
  const [tab, setTab] = useState('stock'), [modal, setModal] = useState(null);
  const [pharmacy, setPharmacy] = useState(''), [q, setQ] = useState(''), [status, setStatus] = useState(''), [offset, setOffset] = useState(0);
  const dq = useDebounced(q);
  const pharmacies = useAsync(() => api.get('/api/pharmacies'), []);
  const stock = useAsync(() => api.get('/api/inventory', { pharmacy_id: pharmacy, q: dq, status, limit: LIMIT, offset }), [pharmacy, dq, status, offset]);
  const alerts = useAsync(() => (tab === 'alerts' ? api.get('/api/inventory/alerts') : Promise.resolve(null)), [tab]);
  const moves = useAsync(() => (tab === 'movements' ? api.get('/api/inventory/movements', { pharmacy_id: pharmacy, limit: 100 }) : Promise.resolve(null)), [tab, pharmacy]);
  const restocks = useAsync(() => (tab === 'history' ? api.get('/api/inventory/restocks') : Promise.resolve(null)), [tab]);
  const plist = (pharmacies.data && pharmacies.data.items) || [];
  const reload = () => { stock.reload(); alerts.reload(); moves.reload(); restocks.reload(); };

  async function writeOff() {
    const target = pharmacy || user.pharmacy_id;
    if (!target) { toast.error('Select a pharmacy first'); return; }
    if (!window.confirm('Write off all expired stock at this pharmacy? This cannot be undone.')) return;
    try { const r = await api.post('/api/inventory/write-off-expired', { pharmacy_id: Number(target) }); toast.success(`${r.units_written_off} expired unit(s) written off`); reload(); } catch (e) { toast.error(e.message); }
  }
  const stockCols = [
    { label: 'Pharmacy', key: 'pharmacy' }, { label: 'Medicine', render: (r) => <span className="font-medium">{r.medicine}</span> }, { label: 'Available', key: 'available' }, { label: 'Reorder at', key: 'reorder_level' },
    { label: 'Expired', render: (r) => (r.expired_qty ? <Badge value="Expired">{r.expired_qty}</Badge> : 0) }, { label: 'Near expiry', render: (r) => (r.near_qty ? <Badge value="Near expiry">{r.near_qty}</Badge> : 0) },
    { label: 'Earliest expiry', key: 'earliest_expiry' }, { label: 'Status', render: (r) => <Badge value={r.status} /> },
    { label: '', render: (r) => (<div className="flex gap-1"><button className="btn-ghost btn-sm" aria-label={'Batches ' + r.medicine} onClick={() => setModal({ type: 'batches', row: r })}><Layers size={12} /></button>
      <button className="btn-ghost btn-sm" onClick={() => setModal({ type: 'restock', row: r })}><Plus size={12} /> Restock</button><button className="btn-ghost btn-sm" aria-label={'Reorder level ' + r.medicine} onClick={() => setModal({ type: 'reorder', row: r })}><Pencil size={12} /></button></div>) }];

  return (
    <div>
      <PageHeader title="Inventory" subtitle="Pharmacy-wise stock, batches, expiry and low-stock alerts.">
        <button className="btn-ghost" onClick={reload}><RefreshCw size={14} /> Refresh</button>
        <button className="btn-ghost" onClick={writeOff}>Write off expired</button>
        <button className="btn-teal" onClick={() => setModal({ type: 'restock' })}><Plus size={14} /> Restock</button>
      </PageHeader>
      <Tabs value={tab} onChange={setTab} tabs={[{ value: 'stock', label: 'Stock levels' }, { value: 'alerts', label: 'Alerts' }, { value: 'movements', label: 'Stock movement' }, { value: 'history', label: 'Restock history' }]} />
      {(tab === 'stock' || tab === 'movements') && user.role === 'Admin' && (
        <div className="mb-3 flex flex-wrap gap-2">
          <select className="input max-w-xs" value={pharmacy} onChange={(e) => { setPharmacy(e.target.value); setOffset(0); }} aria-label="Pharmacy filter"><option value="">All pharmacies</option>{plist.map((p) => <option key={p.pharmacy_id} value={p.pharmacy_id}>{p.name} ({p.city})</option>)}</select>
        </div>)}
      {tab === 'stock' && (<>
        <div className="mb-3 flex flex-wrap gap-2">
          <input className="input max-w-xs" placeholder="Search medicine or generic" value={q} onChange={(e) => { setQ(e.target.value); setOffset(0); }} aria-label="Search stock" />
          <select className="input max-w-[11rem]" value={status} onChange={(e) => { setStatus(e.target.value); setOffset(0); }} aria-label="Stock status"><option value="">All stock</option><option value="low">Low stock</option><option value="out">Out of stock</option><option value="expired">Has expired units</option><option value="near">Nearing expiry</option></select>
        </div>
        {stock.loading && !stock.data ? <Spinner /> : <ErrorBox error={stock.error} onRetry={stock.reload} />}
        {stock.data && (<><DataTable rows={stock.data.items} columns={stockCols} empty="No inventory rows match." /><Pagination total={stock.data.total} limit={LIMIT} offset={offset} onChange={setOffset} /></>)}
      </>)}
      {tab === 'alerts' && (alerts.loading && !alerts.data ? <Spinner /> : alerts.data && (
        <div className="space-y-6">{[['low', 'Low stock', 'Low stock'], ['out', 'Out of stock', 'Out of stock'], ['expired', 'Expired stock', 'Expired'], ['near', 'Nearing expiry', 'Near expiry']].map(([k, title, badge]) => (
          <section key={k}><h3 className="mb-2 flex items-center gap-2 font-semibold">{title} <Badge value={badge}>{alerts.data[k].total}</Badge></h3>
            <DataTable rows={alerts.data[k].items.slice(0, 10)} empty="Nothing to report." columns={stockCols.slice(0, 7)} /></section>))}</div>))}
      {tab === 'movements' && (moves.loading && !moves.data ? <Spinner /> : moves.data && <DataTable rowKey="movement_id" rows={moves.data.items} empty="No stock movements recorded yet. They appear as soon as stock is restocked, distributed or written off." columns={[
        { label: 'When', render: (m) => fmtDate(m.created_at) }, { label: 'Pharmacy', key: 'pharmacy' }, { label: 'Medicine', key: 'medicine' }, { label: 'Change', render: (m) => <span className={m.change_qty < 0 ? 'text-red-600' : 'text-teal-600'}>{m.change_qty > 0 ? '+' : ''}{m.change_qty}</span> },
        { label: 'Reason', key: 'reason' }, { label: 'Reference', key: 'reference' }, { label: 'By', key: 'performed_by' }]} />)}
      {tab === 'history' && (restocks.loading && !restocks.data ? <Spinner /> : restocks.data && <DataTable rowKey="restock_id" rows={restocks.data.items} empty="No restocks recorded yet." columns={[{ label: 'When', render: (m) => fmtDate(m.created_at) }, { label: 'Pharmacy', key: 'pharmacy' }, { label: 'Medicine', key: 'medicine' }, { label: 'Batch', key: 'batch_number' }, { label: 'Units', key: 'quantity' }, { label: 'By', key: 'performed_by' }]} />)}
      {modal?.type === 'restock' && <RestockForm pharmacies={plist} preset={modal.row} onClose={() => setModal(null)} onDone={() => { setModal(null); reload(); }} />}
      {modal?.type === 'reorder' && <ReorderForm row={modal.row} onClose={() => setModal(null)} onDone={() => { setModal(null); stock.reload(); }} />}
      {modal?.type === 'batches' && <Batches row={modal.row} onClose={() => setModal(null)} />}
    </div>
  );
}
