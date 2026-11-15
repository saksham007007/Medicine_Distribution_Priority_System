// KRISHNA - pharmacy management, availability search with approximate distance.
import React, { useState } from 'react';
import { Plus, Pencil, Trash2, Eye, MapPin, Search } from 'lucide-react';
import { api } from '../api.js';
import { useAuth } from '../auth.jsx';
import { useAsync, useDebounced, PageHeader, DataTable, Badge, Modal, Input, Select, ErrorBox, Spinner, SearchSelect, Tabs, useToast } from '../ui.jsx';

function PharmacyForm({ pharmacy, onClose, onDone }) {
  const toast = useToast(); const p = pharmacy;
  const [f, setF] = useState({ name: p?.name || '', address: p?.address || '', city: p?.city || '', state: p?.state || '', pin_code: p?.pin_code || '', phone: p?.phone || '', email: p?.email || '',
    opening_time: p?.opening_time || '09:00', closing_time: p?.closing_time || '21:00', status: p?.status || 'Open', delivery_available: !!p?.delivery_available, latitude: p?.latitude ?? '', longitude: p?.longitude ?? '' });
  const [error, setError] = useState('');
  const set = (k) => (e) => setF({ ...f, [k]: e.target.type === 'checkbox' ? e.target.checked : e.target.value });
  async function submit(e) {
    e.preventDefault(); setError('');
    const body = { ...f };
    ['latitude', 'longitude'].forEach((k) => { if (body[k] === '') delete body[k]; else body[k] = Number(body[k]); });
    ['phone', 'email'].forEach((k) => { if (!body[k]) delete body[k]; });
    try { if (p) await api.put('/api/pharmacies/' + p.pharmacy_id, body); else await api.post('/api/pharmacies', body); toast.success(p ? 'Pharmacy updated' : 'Pharmacy added'); onDone(); } catch (err) { setError(err.message); }
  }
  return (
    <Modal title={p ? 'Edit pharmacy' : 'Add pharmacy'} onClose={onClose} wide>
      <form onSubmit={submit} className="space-y-3">
        <div className="grid gap-3 md:grid-cols-2">
          <Input label="Name" value={f.name} onChange={set('name')} required /><Input label="Address" value={f.address} onChange={set('address')} required />
          <Input label="City" value={f.city} onChange={set('city')} required /><Input label="State" value={f.state} onChange={set('state')} required />
          <Input label="PIN code" value={f.pin_code} onChange={set('pin_code')} required pattern="[0-9]{6}" maxLength={6} /><Input label="Phone" value={f.phone} onChange={set('phone')} placeholder="+91-…" />
          <Input label="Email" type="email" value={f.email} onChange={set('email')} /><Select label="Status" value={f.status} onChange={set('status')} options={['Open', 'Closed', 'Suspended']} />
          <Input label="Opening time" type="time" value={f.opening_time} onChange={set('opening_time')} /><Input label="Closing time" type="time" value={f.closing_time} onChange={set('closing_time')} />
          <Input label="Latitude" type="number" step="any" value={f.latitude} onChange={set('latitude')} /><Input label="Longitude" type="number" step="any" value={f.longitude} onChange={set('longitude')} />
        </div>
        <label className="flex items-center gap-2 text-sm"><input type="checkbox" checked={f.delivery_available} onChange={set('delivery_available')} /> Home delivery available</label>
        <ErrorBox error={error} /><div className="flex justify-end gap-2"><button type="button" className="btn-ghost" onClick={onClose}>Cancel</button><button className="btn-primary">Save</button></div>
      </form>
    </Modal>
  );
}

function PharmacyDetail({ id, onClose }) {
  const { user } = useAuth();
  const staff = user.role !== 'Patient';
  const p = useAsync(() => api.get('/api/pharmacies/' + id), [id]);
  const inv = useAsync(() => (staff ? api.get(`/api/pharmacies/${id}/inventory`, { limit: 25 }) : Promise.resolve(null)), [id, staff]);
  const d = p.data;
  return (
    <Modal title={d ? d.name : 'Pharmacy'} onClose={onClose} wide>
      {p.loading && <Spinner />}<ErrorBox error={p.error} />
      {d && (<div className="space-y-4 text-sm">
        <div className="grid grid-cols-2 gap-3 md:grid-cols-4">
          {[['Address', `${d.address}, ${d.city}, ${d.state} ${d.pin_code}`], ['Phone', d.phone], ['Email', d.email], ['Hours', `${d.opening_time} – ${d.closing_time}`], ['Status', <Badge key="s" value={d.status} />], ['Delivery', d.delivery_available ? 'Available' : 'No'],
            ['Medicines stocked', d.distinct_medicines], ['Total units', d.total_units]].map(([k, v]) => <div key={k}><div className="label">{k}</div>{v || '–'}</div>)}
        </div>
        <p className="text-xs text-slate-500">Sample data: contact details, location and stock are synthetic and not verified live information.</p>
        {staff && inv.data && <section><h3 className="mb-1 font-semibold">Inventory (first {inv.data.items.length} of {inv.data.total})</h3>
          <DataTable rows={inv.data.items} columns={[{ label: 'Medicine', key: 'medicine' }, { label: 'Available', key: 'available' }, { label: 'Reorder at', key: 'reorder_level' }, { label: 'Earliest expiry', key: 'earliest_expiry' }, { label: 'Status', render: (r) => <Badge value={r.status} /> }]} /></section>}
      </div>)}
    </Modal>
  );
}

function FindMedicine() {
  const [medicine, setMedicine] = useState(null), [coords, setCoords] = useState({ lat: '', lon: '' }), [minQty, setMinQty] = useState(1), [geoMsg, setGeoMsg] = useState('');
  const res = useAsync(() => (medicine ? api.get('/api/pharmacies/search', { medicine_id: medicine.medicine_id, lat: coords.lat, lon: coords.lon, min_qty: minQty }) : Promise.resolve(null)), [medicine?.medicine_id, coords.lat, coords.lon, minQty]);
  function locate() {
    if (!navigator.geolocation) { setGeoMsg('Location is not available in this browser.'); return; }
    navigator.geolocation.getCurrentPosition((pos) => { setCoords({ lat: pos.coords.latitude.toFixed(4), lon: pos.coords.longitude.toFixed(4) }); setGeoMsg(''); }, () => setGeoMsg('Could not get your location; enter coordinates manually.'));
  }
  return (
    <div>
      <div className="card mb-4 grid gap-3 md:grid-cols-4">
        <div className="md:col-span-2"><SearchSelect label="Medicine" picked={medicine} onPick={setMedicine} fetcher={(q) => api.get('/api/medicines', { q, limit: 10 }).then((r) => r.items)} renderItem={(m) => `${m.name} · stock ${m.total_stock}`} /></div>
        <Input label="Minimum units" type="number" min="1" value={minQty} onChange={(e) => setMinQty(e.target.value || 1)} />
        <div className="grid grid-cols-2 gap-2"><Input label="Latitude" type="number" step="any" value={coords.lat} onChange={(e) => setCoords({ ...coords, lat: e.target.value })} /><Input label="Longitude" type="number" step="any" value={coords.lon} onChange={(e) => setCoords({ ...coords, lon: e.target.value })} /></div>
        <div className="md:col-span-4 flex items-center gap-3"><button type="button" className="btn-ghost btn-sm" onClick={locate}><MapPin size={12} /> Use my location</button><span className="text-xs text-slate-500">{geoMsg || 'Add coordinates to sort by approximate straight-line distance.'}</span></div>
      </div>
      {!medicine && <div className="rounded-lg border border-dashed border-slate-300 p-6 text-center text-sm text-slate-500">Choose a medicine to find pharmacies that stock it.</div>}
      {res.loading && medicine && <Spinner />}<ErrorBox error={res.error} />
      {res.data && (<>
        <DataTable rowKey="pharmacy_id" rows={res.data.pharmacies} empty="No open pharmacy currently has this medicine in the sample data. Check alternatives." columns={[
          { label: 'Pharmacy', render: (p) => <span className="font-medium">{p.name}</span> }, { label: 'Location', render: (p) => `${p.address}, ${p.city}` }, { label: 'Units', key: 'available' },
          { label: 'Distance', render: (p) => (p.distance_km === null ? '–' : p.distance_km + ' km') }, { label: 'Hours', render: (p) => `${p.opening_time}–${p.closing_time}` }, { label: 'Delivery', render: (p) => (p.delivery_available ? 'Yes' : 'No') }, { label: 'Phone', key: 'phone' }]} />
        <p className="mt-2 text-xs text-slate-500">{res.data.note}</p></>)}
    </div>
  );
}

export default function Pharmacies() {
  const { user } = useAuth(); const toast = useToast();
  const admin = user.role === 'Admin';
  const [tab, setTab] = useState('all'), [q, setQ] = useState(''), [status, setStatus] = useState(''), [modal, setModal] = useState(null);
  const dq = useDebounced(q);
  const list = useAsync(() => api.get('/api/pharmacies', { q: dq, status }), [dq, status]);
  async function remove(p) {
    if (!window.confirm(`Delete ${p.name}?`)) return;
    try { await api.del('/api/pharmacies/' + p.pharmacy_id); toast.success('Pharmacy deleted'); list.reload(); } catch (e) { toast.error(e.message); }
  }
  return (
    <div>
      <PageHeader title="Pharmacies" subtitle="30+ sample pharmacies. Availability is sample data, not live stock.">
        {admin && <button className="btn-primary" onClick={() => setModal({ type: 'form' })}><Plus size={14} /> Add pharmacy</button>}
      </PageHeader>
      <Tabs value={tab} onChange={setTab} tabs={[{ value: 'all', label: 'All pharmacies' }, { value: 'find', label: 'Find a medicine' }]} />
      {tab === 'find' && <FindMedicine />}
      {tab === 'all' && (<>
        <div className="mb-3 flex flex-wrap gap-2">
          <div className="relative max-w-xs flex-1"><Search size={14} className="absolute left-3 top-3 text-slate-400" /><input className="input pl-8" placeholder="Search name, city, PIN" value={q} onChange={(e) => setQ(e.target.value)} aria-label="Search pharmacies" /></div>
          <select className="input max-w-[10rem]" value={status} onChange={(e) => setStatus(e.target.value)} aria-label="Pharmacy status"><option value="">All statuses</option><option>Open</option><option>Closed</option><option>Suspended</option></select>
        </div>
        {list.loading && !list.data ? <Spinner /> : <ErrorBox error={list.error} onRetry={list.reload} />}
        {list.data && <DataTable rowKey="pharmacy_id" rows={list.data.items} columns={[
          { label: 'Name', render: (p) => <span className="font-medium">{p.name}</span> }, { label: 'City', key: 'city' }, { label: 'Phone', key: 'phone' }, { label: 'Hours', render: (p) => `${p.opening_time}–${p.closing_time}` },
          { label: 'Delivery', render: (p) => (p.delivery_available ? 'Yes' : 'No') }, { label: 'Items', key: 'items_in_stock' }, { label: 'Status', render: (p) => <Badge value={p.status} /> },
          { label: '', render: (p) => (<div className="flex gap-1"><button className="btn-ghost btn-sm" aria-label={'Details ' + p.name} onClick={() => setModal({ type: 'detail', id: p.pharmacy_id })}><Eye size={12} /></button>
            {admin && <><button className="btn-ghost btn-sm" aria-label={'Edit ' + p.name} onClick={() => setModal({ type: 'form', p })}><Pencil size={12} /></button><button className="btn-ghost btn-sm" aria-label={'Delete ' + p.name} onClick={() => remove(p)}><Trash2 size={12} /></button></>}</div>) }]} />}
        {list.data && <p className="mt-2 text-xs text-slate-500">{list.data.total} pharmacies</p>}
      </>)}
      {modal?.type === 'detail' && <PharmacyDetail id={modal.id} onClose={() => setModal(null)} />}
      {modal?.type === 'form' && <PharmacyForm pharmacy={modal.p} onClose={() => setModal(null)} onDone={() => { setModal(null); list.reload(); }} />}
    </div>
  );
}
