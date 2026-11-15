// NIKITA - patient management (staff) and "My Profile" (patients).
import React, { useState } from 'react';
import { Plus, Pencil, Trash2, Eye, Search } from 'lucide-react';
import { api } from '../api.js';
import { useAuth } from '../auth.jsx';
import { useAsync, useDebounced, PageHeader, DataTable, Pagination, Badge, Modal, Input, Select, ErrorBox, Spinner, useToast, fmtDate } from '../ui.jsx';

const LIMIT = 15;

function PatientForm({ patient, onDone, onClose }) {
  const toast = useToast();
  const [f, setF] = useState({ name: patient?.name || '', age: patient?.age ?? '', gender: patient?.gender || '', contact: patient?.contact || '', email: patient?.email || '', address: patient?.address || '', city: patient?.city || '' });
  const [error, setError] = useState(''), [busy, setBusy] = useState(false);
  const set = (k) => (e) => setF({ ...f, [k]: e.target.value });
  async function submit(e) {
    e.preventDefault(); setBusy(true); setError('');
    const body = { name: f.name };
    ['gender', 'contact', 'email', 'address', 'city'].forEach((k) => { if (f[k] !== '') body[k] = f[k]; });
    if (f.age !== '') body.age = Number(f.age);
    try {
      if (patient) await api.put('/api/patients/' + patient.patient_id, body); else await api.post('/api/patients', body);
      toast.success(patient ? 'Patient updated' : 'Patient created'); onDone();
    } catch (err) { setError(err.message); } finally { setBusy(false); }
  }
  return (
    <Modal title={patient ? 'Edit patient' : 'Register patient'} onClose={onClose}>
      <form onSubmit={submit} className="space-y-3">
        <Input label="Full name" value={f.name} onChange={set('name')} required minLength={2} />
        <div className="grid grid-cols-2 gap-3">
          <Input label="Age" type="number" min="0" max="120" value={f.age} onChange={set('age')} />
          <Select label="Gender" value={f.gender} onChange={set('gender')} placeholder="—" options={['Male', 'Female', 'Other']} />
        </div>
        <div className="grid grid-cols-2 gap-3">
          <Input label="Contact" value={f.contact} onChange={set('contact')} placeholder="+91-…" />
          <Input label="Email" type="email" value={f.email} onChange={set('email')} />
        </div>
        <Input label="Address" value={f.address} onChange={set('address')} />
        <Input label="City" value={f.city} onChange={set('city')} />
        <ErrorBox error={error} />
        <div className="flex justify-end gap-2"><button type="button" className="btn-ghost" onClick={onClose}>Cancel</button><button className="btn-primary" disabled={busy}>{busy ? 'Saving…' : 'Save'}</button></div>
      </form>
    </Modal>
  );
}

export function PatientHistory({ patientId, onClose }) {
  const h = useAsync(() => api.get(`/api/patients/${patientId}/history`), [patientId]);
  return (
    <Modal title={h.data ? 'History · ' + h.data.patient.name : 'Patient history'} onClose={onClose} wide>
      {h.loading && <Spinner />}<ErrorBox error={h.error} />
      {h.data && (
        <div className="space-y-5">
          <section><h3 className="mb-2 font-semibold">Medicine requests</h3>
            <DataTable rowKey="request_id" rows={h.data.requests} empty="No requests yet." columns={[{ label: '#', key: 'request_id' }, { label: 'Medicine', key: 'medicine' }, { label: 'Qty', key: 'quantity' },
              { label: 'Urgency', render: (r) => <Badge value={r.urgency} /> }, { label: 'Status', render: (r) => <Badge value={r.status} /> }, { label: 'Requested', render: (r) => fmtDate(r.requested_at) }]} /></section>
          <section><h3 className="mb-2 font-semibold">Distribution status</h3>
            <DataTable rowKey="transaction_id" rows={h.data.transactions} empty="No distributions yet." columns={[{ label: 'Txn', key: 'transaction_id' }, { label: 'Medicine', key: 'medicine_name' }, { label: 'Qty', key: 'quantity' },
              { label: 'Pharmacy', render: (t) => t.pharmacy || '–' }, { label: 'Status', render: (t) => <Badge value={t.status} /> }, { label: 'When', render: (t) => fmtDate(t.created_at) }]} /></section>
          <section><h3 className="mb-2 font-semibold">Prescriptions</h3>
            <DataTable rowKey="prescription_id" rows={h.data.prescriptions} empty="No prescriptions." columns={[{ label: '#', key: 'prescription_id' }, { label: 'Medicine', key: 'medicine' }, { label: 'Doctor', key: 'doctor_name' },
              { label: 'Issued', key: 'issued_date' }, { label: 'Verified', render: (p) => (p.verified ? <Badge value="Approved">Verified</Badge> : <Badge value="Proposed">Pending</Badge>) }]} /></section>
        </div>)}
    </Modal>
  );
}

function MyProfile() {
  const { user } = useAuth();
  const p = useAsync(() => api.get('/api/patients/' + user.patient_id), [user.patient_id]);
  const [edit, setEdit] = useState(false), [hist, setHist] = useState(false);
  if (p.loading && !p.data) return <Spinner />;
  if (!p.data) return <ErrorBox error={p.error || 'No patient record is linked to this account.'} onRetry={p.reload} />;
  const d = p.data;
  return (
    <div>
      <PageHeader title="My Profile" subtitle="Your details and medicine history">
        <button className="btn-ghost" onClick={() => setHist(true)}><Eye size={14} /> My history</button><button className="btn-primary" onClick={() => setEdit(true)}><Pencil size={14} /> Edit details</button>
      </PageHeader>
      <div className="card grid max-w-2xl grid-cols-2 gap-3 text-sm" data-testid="profile">
        {[['Name', d.name], ['Age', d.age], ['Gender', d.gender], ['Contact', d.contact], ['Email', d.email], ['City', d.city], ['Address', d.address], ['Registered', fmtDate(d.created_at)]].map(([k, v]) => <div key={k}><div className="label">{k}</div>{v || '–'}</div>)}
      </div>
      {edit && <PatientForm patient={d} onClose={() => setEdit(false)} onDone={() => { setEdit(false); p.reload(); }} />}
      {hist && <PatientHistory patientId={d.patient_id} onClose={() => setHist(false)} />}
    </div>
  );
}

function PatientList() {
  const { user } = useAuth(); const toast = useToast();
  const [q, setQ] = useState(''), [city, setCity] = useState(''), [offset, setOffset] = useState(0), [modal, setModal] = useState(null), [hist, setHist] = useState(null);
  const dq = useDebounced(q);
  const list = useAsync(() => api.get('/api/patients', { q: dq, city, limit: LIMIT, offset }), [dq, city, offset]);
  async function remove(p) {
    if (!window.confirm(`Deactivate ${p.name}? Their history is kept.`)) return;
    try { await api.del('/api/patients/' + p.patient_id); toast.success('Patient deactivated'); list.reload(); } catch (e) { toast.error(e.message); }
  }
  return (
    <div>
      <PageHeader title="Patients" subtitle="Register, search and review patients">
        <button className="btn-primary" onClick={() => setModal({})}><Plus size={14} /> Register patient</button>
      </PageHeader>
      <div className="mb-3 flex flex-wrap gap-2">
        <div className="relative max-w-xs flex-1"><Search size={14} className="absolute left-3 top-3 text-slate-400" /><input className="input pl-8" placeholder="Search name, contact, email" value={q} onChange={(e) => { setQ(e.target.value); setOffset(0); }} aria-label="Search patients" /></div>
        <input className="input max-w-[11rem]" placeholder="City" value={city} onChange={(e) => { setCity(e.target.value); setOffset(0); }} aria-label="Filter city" />
      </div>
      {list.loading && !list.data ? <Spinner /> : <ErrorBox error={list.error} onRetry={list.reload} />}
      {list.data && (<>
        <DataTable rowKey="patient_id" rows={list.data.items} columns={[
          { label: 'ID', key: 'patient_id' }, { label: 'Name', key: 'name' }, { label: 'Age', key: 'age' }, { label: 'Contact', key: 'contact' }, { label: 'City', key: 'city' },
          { label: 'Open requests', render: (p) => (p.open_requests ? <Badge value="Pending">{p.open_requests}</Badge> : '0') },
          { label: '', render: (p) => (
            <div className="flex gap-1">
              <button className="btn-ghost btn-sm" aria-label={'History ' + p.name} onClick={() => setHist(p.patient_id)}><Eye size={12} /></button>
              <button className="btn-ghost btn-sm" aria-label={'Edit ' + p.name} onClick={() => setModal(p)}><Pencil size={12} /></button>
              {user.role === 'Admin' && <button className="btn-ghost btn-sm" aria-label={'Deactivate ' + p.name} onClick={() => remove(p)}><Trash2 size={12} /></button>}
            </div>) }]} />
        <Pagination total={list.data.total} limit={LIMIT} offset={offset} onChange={setOffset} />
      </>)}
      {modal && <PatientForm patient={modal.patient_id ? modal : null} onClose={() => setModal(null)} onDone={() => { setModal(null); list.reload(); }} />}
      {hist && <PatientHistory patientId={hist} onClose={() => setHist(null)} />}
    </div>
  );
}

export default function Patients() {
  const { user } = useAuth();
  return user.role === 'Patient' ? <MyProfile /> : <PatientList />;
}
