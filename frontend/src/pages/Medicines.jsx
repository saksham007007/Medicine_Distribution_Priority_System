// VANSH - medicine catalogue: search, filter, sort, details, and Admin CRUD.
import React, { useState } from 'react';
import { Plus, Pencil, Trash2, Eye, Search } from 'lucide-react';
import { api } from '../api.js';
import { useAuth } from '../auth.jsx';
import { useAsync, useDebounced, PageHeader, DataTable, Pagination, Badge, Modal, Input, Select, Textarea, ErrorBox, Spinner, useToast, money, fmtDate } from '../ui.jsx';

const LIMIT = 20;

function MedicineForm({ medicine, categories, manufacturers, onDone, onClose }) {
  const toast = useToast();
  const m = medicine;
  const [f, setF] = useState({ name: m?.name || '', generic_name: m?.generic_name || '', brand_name: m?.brand_name || '', category: m?.category || '', manufacturer: m?.manufacturer || '',
    composition: m?.composition || '', strength: m?.strength || '', dosage_form: m?.dosage_form || '', unit_price: m?.unit_price ?? '', min_stock: m?.min_stock ?? 20,
    requires_prescription: !!m?.requires_prescription, description: m?.description || '', storage_instructions: m?.storage_instructions || '', status: m?.status || 'Active' });
  const [error, setError] = useState(''), [busy, setBusy] = useState(false);
  const set = (k) => (e) => setF({ ...f, [k]: e.target.type === 'checkbox' ? e.target.checked : e.target.value });
  async function submit(e) {
    e.preventDefault(); setBusy(true); setError('');
    const body = { ...f, unit_price: Number(f.unit_price), min_stock: Number(f.min_stock) };
    if (!m) delete body.status;
    try {
      if (m) await api.put('/api/medicines/' + m.medicine_id, body); else await api.post('/api/medicines', body);
      toast.success(m ? 'Medicine updated' : 'Medicine added'); onDone();
    } catch (err) { setError(err.message); } finally { setBusy(false); }
  }
  return (
    <Modal title={m ? 'Edit medicine' : 'Add medicine'} onClose={onClose} wide>
      <form onSubmit={submit} className="space-y-3">
        <datalist id="cats">{categories.map((c) => <option key={c.name} value={c.name} />)}</datalist>
        <datalist id="mfgs">{manufacturers.map((c) => <option key={c.name} value={c.name} />)}</datalist>
        <div className="grid gap-3 md:grid-cols-2">
          <Input label="Display name" value={f.name} onChange={set('name')} required hint="Must be unique, e.g. Brand 500mg Tablet" />
          <Input label="Brand name" value={f.brand_name} onChange={set('brand_name')} required />
          <Input label="Generic name" value={f.generic_name} onChange={set('generic_name')} required />
          <Input label="Composition" value={f.composition} onChange={set('composition')} required />
          <Input label="Category" list="cats" value={f.category} onChange={set('category')} required />
          <Input label="Manufacturer" list="mfgs" value={f.manufacturer} onChange={set('manufacturer')} required />
          <Input label="Strength" value={f.strength} onChange={set('strength')} required />
          <Input label="Dosage form" value={f.dosage_form} onChange={set('dosage_form')} required />
          <Input label="Unit price (₹)" type="number" step="0.01" min="0" value={f.unit_price} onChange={set('unit_price')} required />
          <Input label="Minimum stock" type="number" min="0" value={f.min_stock} onChange={set('min_stock')} />
          {m && <Select label="Status" value={f.status} onChange={set('status')} options={['Active', 'Discontinued']} />}
          <label className="flex items-center gap-2 pt-6 text-sm"><input type="checkbox" checked={f.requires_prescription} onChange={set('requires_prescription')} /> Prescription required</label>
        </div>
        <Textarea label="Description" value={f.description} onChange={set('description')} />
        <Textarea label="Storage instructions" value={f.storage_instructions} onChange={set('storage_instructions')} />
        <ErrorBox error={error} />
        <div className="flex justify-end gap-2"><button type="button" className="btn-ghost" onClick={onClose}>Cancel</button><button className="btn-primary" disabled={busy}>{busy ? 'Saving…' : 'Save'}</button></div>
      </form>
    </Modal>
  );
}

export function MedicineDetail({ id, onClose }) {
  const d = useAsync(() => api.get('/api/medicines/' + id), [id]);
  const m = d.data;
  return (
    <Modal title={m ? m.name : 'Medicine details'} onClose={onClose} wide>
      {d.loading && <Spinner />}<ErrorBox error={d.error} />
      {m && (
        <div className="space-y-4 text-sm">
          <div className="grid grid-cols-2 gap-3 md:grid-cols-4">
            {[['Generic name', m.generic_name], ['Brand', m.brand_name], ['Category', m.category], ['Manufacturer', m.manufacturer], ['Composition', m.composition], ['Strength', m.strength], ['Dosage form', m.dosage_form], ['Unit price', money(m.unit_price)],
              ['Prescription', m.requires_prescription ? 'Required' : 'Not required'], ['Status', <Badge key="s" value={m.status} />], ['Total stock', m.total_stock], ['Min stock', m.min_stock]].map(([k, v]) => <div key={k}><div className="label">{k}</div>{v}</div>)}
          </div>
          {m.description && <p className="text-slate-600">{m.description}</p>}
          <p className="text-xs text-slate-500">Storage: {m.storage_instructions}</p>
          <section><h3 className="mb-1 font-semibold">Available at pharmacies (sample data)</h3>
            <DataTable rowKey="pharmacy_id" rows={m.availability} empty="Not currently available at any pharmacy." columns={[{ label: 'Pharmacy', key: 'pharmacy' }, { label: 'City', key: 'city' }, { label: 'Status', render: (a) => <Badge value={a.status} /> },
              { label: 'Units', key: 'available' }, { label: 'Earliest expiry', key: 'earliest_expiry' }, { label: 'Phone', key: 'phone' }]} /></section>
        </div>)}
    </Modal>
  );
}

export default function Medicines() {
  const { user } = useAuth(); const toast = useToast();
  const admin = user.role === 'Admin';
  const [f, setF] = useState({ q: '', category: '', form: '', rx: '', available: false, sort: 'name', order: 'asc' });
  const [offset, setOffset] = useState(0), [modal, setModal] = useState(null);
  const dq = useDebounced(f.q);
  const cats = useAsync(() => api.get('/api/categories'), []);
  const forms = useAsync(() => api.get('/api/dosage-forms'), []);
  const mfgs = useAsync(() => (admin ? api.get('/api/manufacturers') : Promise.resolve({ items: [] })), [admin]);
  const list = useAsync(() => api.get('/api/medicines', { q: dq, category: f.category, form: f.form, rx: f.rx, available: f.available ? 1 : '', sort: f.sort, order: f.order, limit: LIMIT, page: Math.floor(offset / LIMIT) + 1, status: admin ? 'all' : '' }),
    [dq, f.category, f.form, f.rx, f.available, f.sort, f.order, offset]);
  const set = (k) => (e) => { setF({ ...f, [k]: e.target.type === 'checkbox' ? e.target.checked : e.target.value }); setOffset(0); };

  async function edit(id) { try { setModal({ type: 'form', medicine: await api.get('/api/medicines/' + id) }); } catch (e) { toast.error(e.message); } }
  async function remove(m) {
    if (!window.confirm(`Remove ${m.name}? Medicines with history are marked Discontinued instead of deleted.`)) return;
    try { const r = await api.del('/api/medicines/' + m.medicine_id); toast.success(r.message); list.reload(); } catch (e) { toast.error(e.message); }
  }
  return (
    <div>
      <PageHeader title="Medicines" subtitle="Search the catalogue (sample demonstration data) and check availability.">
        {admin && <button className="btn-primary" onClick={() => setModal({ type: 'form' })}><Plus size={14} /> Add medicine</button>}
      </PageHeader>
      <div className="card mb-3 grid gap-3 md:grid-cols-4">
        <div className="relative md:col-span-2"><span className="label">Search name, generic, brand or manufacturer</span><Search size={14} className="absolute left-3 top-9 text-slate-400" /><input className="input pl-8" value={f.q} onChange={set('q')} placeholder="e.g. paracetamol" aria-label="Search medicines" /></div>
        <Select label="Category" value={f.category} onChange={set('category')} placeholder="All categories" options={((cats.data && cats.data.items) || []).map((c) => c.name)} aria-label="Category" />
        <Select label="Dosage form" value={f.form} onChange={set('form')} placeholder="All forms" options={((forms.data && forms.data.items) || []).map((c) => c.name)} aria-label="Dosage form" />
        <Select label="Prescription" value={f.rx} onChange={set('rx')} placeholder="Any" options={[{ value: '1', label: 'Prescription only' }, { value: '0', label: 'Over the counter' }]} aria-label="Prescription filter" />
        <Select label="Sort by" value={f.sort} onChange={set('sort')} options={[{ value: 'name', label: 'Name' }, { value: 'price', label: 'Price' }, { value: 'availability', label: 'Availability' }, { value: 'generic', label: 'Generic name' }]} aria-label="Sort by" />
        <Select label="Order" value={f.order} onChange={set('order')} options={[{ value: 'asc', label: 'Ascending' }, { value: 'desc', label: 'Descending' }]} aria-label="Order" />
        <label className="flex items-center gap-2 pt-6 text-sm"><input type="checkbox" checked={f.available} onChange={set('available')} /> In stock only</label>
      </div>
      {list.loading && !list.data ? <Spinner /> : <ErrorBox error={list.error} onRetry={list.reload} />}
      {list.data && (<>
        <DataTable rowKey="medicine_id" rows={list.data.items} empty="No medicines match these filters." columns={[
          { label: 'Name', render: (m) => <span className="font-medium">{m.name}</span> }, { label: 'Generic', key: 'generic_name' }, { label: 'Category', key: 'category' }, { label: 'Form', key: 'dosage_form' },
          { label: 'Rx', render: (m) => (m.requires_prescription ? <Badge value="Proposed">Rx</Badge> : <span className="text-slate-400">OTC</span>) }, { label: 'Price', render: (m) => money(m.unit_price) },
          { label: 'Stock', render: (m) => (m.total_stock === 0 ? <Badge value="Out of stock" /> : <span>{m.total_stock}{m.low_stock && <> <Badge value="Low stock" /></>}</span>) },
          ...(admin ? [{ label: 'Status', render: (m) => <Badge value={m.status} /> }] : []),
          { label: '', render: (m) => (
            <div className="flex gap-1"><button className="btn-ghost btn-sm" aria-label={'Details ' + m.name} onClick={() => setModal({ type: 'detail', id: m.medicine_id })}><Eye size={12} /></button>
              {admin && <><button className="btn-ghost btn-sm" aria-label={'Edit ' + m.name} onClick={() => edit(m.medicine_id)}><Pencil size={12} /></button><button className="btn-ghost btn-sm" aria-label={'Remove ' + m.name} onClick={() => remove(m)}><Trash2 size={12} /></button></>}</div>) }]} />
        <Pagination total={list.data.total} limit={LIMIT} offset={offset} onChange={setOffset} />
      </>)}
      {modal?.type === 'detail' && <MedicineDetail id={modal.id} onClose={() => setModal(null)} />}
      {modal?.type === 'form' && <MedicineForm medicine={modal.medicine} categories={(cats.data && cats.data.items) || []} manufacturers={(mfgs.data && mfgs.data.items) || []} onClose={() => setModal(null)} onDone={() => { setModal(null); list.reload(); }} />}
    </div>
  );
}
