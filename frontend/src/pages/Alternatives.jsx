// KRISHNA - alternative medicines: suggestions, comparison, and pharmacist-approved substitution.
import React, { useState } from 'react';
import { ShieldCheck } from 'lucide-react';
import { api } from '../api.js';
import { useAuth } from '../auth.jsx';
import { useAsync, PageHeader, DataTable, Badge, Modal, Select, ErrorBox, Spinner, SearchSelect, useToast, money } from '../ui.jsx';

function CompareModal({ a, b, onClose }) {
  const c = useAsync(() => api.get('/api/alternatives/compare', { a, b }), [a, b]);
  const rows = [['Name', 'name'], ['Generic name', 'generic_name'], ['Brand', 'brand_name'], ['Composition', 'composition'], ['Strength', 'strength'], ['Dosage form', 'dosage_form'], ['Category', 'category'], ['Manufacturer', 'manufacturer'], ['Unit price', 'unit_price'], ['Units in stock', 'available_units']];
  return (
    <Modal title="Compare medicines" onClose={onClose} wide>
      {c.loading && <Spinner />}<ErrorBox error={c.error} />
      {c.data && (<>
        <table className="min-w-full text-sm"><thead><tr><th className="th">Attribute</th><th className="th">Requested</th><th className="th">Alternative</th></tr></thead><tbody className="divide-y divide-slate-100">
          {rows.map(([label, k]) => { const [x, y] = c.data.medicines; const same = String(x[k]) === String(y[k]); return (<tr key={k}><td className="td font-medium">{label}</td><td className="td">{k === 'unit_price' ? money(x[k]) : x[k]}</td><td className={'td ' + (same ? '' : 'bg-amber-50')}>{k === 'unit_price' ? money(y[k]) : y[k]}</td></tr>); })}
          <tr><td className="td font-medium">Prescription</td><td className="td">{c.data.medicines[0].requires_prescription ? 'Required' : 'Not required'}</td><td className="td">{c.data.medicines[1].requires_prescription ? 'Required' : 'Not required'}</td></tr>
        </tbody></table>
        <div className="mt-3 flex flex-wrap gap-2 text-xs">{Object.entries(c.data.comparison).map(([k, v]) => <Badge key={k} value={v ? 'OK' : 'Proposed'}>{k.replace(/_/g, ' ')}: {v ? 'yes' : 'no'}</Badge>)}</div>
        <p className="mt-3 text-xs text-slate-500">{c.data.disclaimer}</p></>)}
    </Modal>
  );
}

function ProposeModal({ alt, requests, onClose, onDone }) {
  const toast = useToast();
  const [rid, setRid] = useState(requests.length === 1 ? String(requests[0].request_id) : ''), [error, setError] = useState('');
  async function submit(e) {
    e.preventDefault(); setError('');
    try { await api.post(`/api/requests/${rid}/substitution`, { alternative_id: alt.medicine_id }); toast.success('Alternative proposed. A pharmacist must approve it before anything is substituted.'); onDone(); } catch (err) { setError(err.message); }
  }
  return (
    <Modal title={'Propose ' + alt.name} onClose={onClose}>
      <form onSubmit={submit} className="space-y-3">
        <p className="text-sm text-slate-600">Matching type: <b>{alt.match_type}</b>. The patient is notified, but nothing is substituted until a pharmacist approves.</p>
        <Select label="Waiting request" value={rid} onChange={(e) => setRid(e.target.value)} required placeholder="Select request…" options={requests.map((r) => ({ value: r.request_id, label: `#${r.request_id} · ${r.patient_name} · qty ${r.quantity} · ${r.urgency}` }))} />
        <ErrorBox error={error} /><div className="flex justify-end gap-2"><button type="button" className="btn-ghost" onClick={onClose}>Cancel</button><button className="btn-primary" disabled={!rid}>Propose alternative</button></div>
      </form>
    </Modal>
  );
}

export function AlternativesPanel({ medicineId, onChanged }) {
  const { user } = useAuth();
  const staff = user.role !== 'Patient';
  const [cmp, setCmp] = useState(null), [prop, setProp] = useState(null);
  const alts = useAsync(() => api.get(`/api/medicines/${medicineId}/alternatives`), [medicineId]);
  const waiting = useAsync(() => (staff ? api.get('/api/waiting-list') : Promise.resolve({ items: [] })), [staff, medicineId]);
  if (alts.loading && !alts.data) return <Spinner />;
  if (!alts.data) return <ErrorBox error={alts.error} onRetry={alts.reload} />;
  const open = ((waiting.data && waiting.data.items) || []).filter((w) => w.medicine_id === medicineId && w.status === 'Waiting' && w.substitution_status !== 'Proposed' && w.substitution_status !== 'Approved');
  return (
    <div className="space-y-3">
      <div className="flex items-start gap-2 rounded-lg border border-amber-200 bg-amber-50 p-3 text-sm text-amber-800" data-testid="disclaimer"><ShieldCheck size={18} className="mt-0.5 shrink-0" /><span>{alts.data.disclaimer}</span></div>
      <DataTable rowKey="medicine_id" rows={alts.data.alternatives} empty="No registered alternatives for this medicine." columns={[
        { label: 'Alternative', render: (a) => <div><div className="font-medium">{a.name}</div><div className="text-xs text-slate-500">{a.manufacturer}</div></div> },
        { label: 'Match', render: (a) => <Badge value={a.match_type === 'Same composition' ? 'OK' : 'Proposed'}>{a.match_type}</Badge> }, { label: 'Strength / form', render: (a) => `${a.strength} · ${a.dosage_form}` },
        { label: 'Rx', render: (a) => (a.requires_prescription ? 'Required' : 'No') }, { label: 'Price Δ', render: (a) => (a.price_difference > 0 ? '+' : '') + money(a.price_difference) },
        { label: 'Available', render: (a) => (a.available_units > 0 ? <span>{a.available_units} units · {a.pharmacy_count} pharmacies</span> : <Badge value="Out of stock" />) },
        { label: 'Where', render: (a) => a.pharmacies.map((p) => p.name + ' (' + p.city + ')').join('; ') || '–' },
        { label: '', render: (a) => (<div className="flex gap-1"><button className="btn-ghost btn-sm" onClick={() => setCmp(a.medicine_id)}>Compare</button>
          {staff && <button className="btn-teal btn-sm" disabled={open.length === 0} title={open.length === 0 ? 'No waiting request for the requested medicine' : ''} onClick={() => setProp({ alt: a })}>Propose</button>}</div>) }]} />
      {staff && <p className="text-xs text-slate-500">{open.length} waiting request(s) for this medicine can receive a proposal.</p>}
      {cmp && <CompareModal a={medicineId} b={cmp} onClose={() => setCmp(null)} />}
      {prop && <ProposeModal alt={prop.alt} requests={open} onClose={() => setProp(null)} onDone={() => { setProp(null); waiting.reload(); if (onChanged) onChanged(); }} />}
    </div>
  );
}

export default function Alternatives() {
  const [medicine, setMedicine] = useState(null);
  return (
    <div>
      <PageHeader title="Medicine Alternatives" subtitle="Suggestions matched on generic name, strength and dosage form. A pharmacist must confirm any substitution." />
      <div className="card mb-4 max-w-xl">
        <SearchSelect label="Medicine to find alternatives for" picked={medicine} onPick={setMedicine} fetcher={(q) => api.get('/api/medicines', { q, limit: 10 }).then((r) => r.items)}
          renderItem={(m) => `${m.name} · stock ${m.total_stock}`} placeholder="Search by name or generic…" />
      </div>
      {medicine ? <AlternativesPanel key={medicine.medicine_id} medicineId={medicine.medicine_id} /> : <div className="rounded-lg border border-dashed border-slate-300 p-6 text-center text-sm text-slate-500">Select a medicine to see suggested alternatives and where they are available.</div>}
    </div>
  );
}
