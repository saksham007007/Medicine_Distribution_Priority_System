// NIKITA - priority queue, medicine requests and prescriptions.
import React, { useState } from 'react';
import { Plus, RefreshCw, ArrowUpDown, Eye } from 'lucide-react';
import { api } from '../api.js';
import { useAuth } from '../auth.jsx';
import { useAsync, PageHeader, DataTable, Pagination, Badge, Modal, Input, Select, Textarea, ErrorBox, Spinner, SearchSelect, Tabs, useToast, fmtDate, minutes } from '../ui.jsx';

const URGENCY = ['Critical', 'High', 'Medium', 'Normal'];
const today = () => new Date().toISOString().slice(0, 10);

function PrescriptionForm({ patientId, medicine, onClose, onDone }) {
  const toast = useToast();
  const [f, setF] = useState({ doctor_name: '', doctor_reg_no: '', issued_date: today(), quantity: 10, notes: '' });
  const [error, setError] = useState('');
  const set = (k) => (e) => setF({ ...f, [k]: e.target.value });
  async function submit(e) {
    e.preventDefault(); setError('');
    try {
      const body = { ...f, quantity: Number(f.quantity), medicine_id: medicine.medicine_id };
      if (patientId) body.patient_id = patientId;
      const r = await api.post('/api/prescriptions', body); toast.success('Prescription recorded (awaiting pharmacist verification)'); onDone(r.prescription_id);
    } catch (err) { setError(err.message); }
  }
  return (
    <Modal title={'Add prescription · ' + medicine.name} onClose={onClose}>
      <form onSubmit={submit} className="space-y-3">
        <Input label="Doctor name" value={f.doctor_name} onChange={set('doctor_name')} required minLength={3} />
        <div className="grid grid-cols-2 gap-3"><Input label="Registration no." value={f.doctor_reg_no} onChange={set('doctor_reg_no')} /><Input label="Issued date" type="date" max={today()} value={f.issued_date} onChange={set('issued_date')} required /></div>
        <Input label="Prescribed quantity" type="number" min="1" value={f.quantity} onChange={set('quantity')} required />
        <Textarea label="Notes" value={f.notes} onChange={set('notes')} />
        <ErrorBox error={error} /><div className="flex justify-end gap-2"><button type="button" className="btn-ghost" onClick={onClose}>Cancel</button><button className="btn-primary">Save prescription</button></div>
      </form>
    </Modal>
  );
}

function RequestForm({ onClose, onDone }) {
  const { user } = useAuth(); const toast = useToast();
  const staff = user.role !== 'Patient';
  const [patient, setPatient] = useState(null), [medicine, setMedicine] = useState(null);
  const [qty, setQty] = useState(1), [urgency, setUrgency] = useState('Normal'), [notes, setNotes] = useState(''), [rx, setRx] = useState('');
  const [error, setError] = useState(''), [busy, setBusy] = useState(false), [addRx, setAddRx] = useState(false);
  const patientId = staff ? patient?.patient_id : user.patient_id;
  const rxList = useAsync(() => (medicine && medicine.requires_prescription && patientId ? api.get('/api/prescriptions', { patient_id: patientId }) : Promise.resolve({ items: [] })), [medicine?.medicine_id, patientId]);
  const usable = ((rxList.data && rxList.data.items) || []).filter((p) => medicine && p.medicine_id === medicine.medicine_id);
  async function submit(e) {
    e.preventDefault(); setBusy(true); setError('');
    try {
      const body = { medicine_id: medicine.medicine_id, quantity: Number(qty), notes };
      if (staff) { body.patient_id = patient.patient_id; body.urgency = urgency; }
      if (rx) body.prescription_id = Number(rx);
      const r = await api.post('/api/requests', body);
      toast.success(`Request #${r.request_id} queued at position ${r.queue_position}${r.in_stock_now ? '' : ' (currently out of stock - it will wait)'}`); onDone();
    } catch (err) { setError(err.message); } finally { setBusy(false); }
  }
  return (
    <Modal title="New medicine request" onClose={onClose}>
      <form onSubmit={submit} className="space-y-3">
        {staff && <SearchSelect label="Patient" picked={patient} onPick={setPatient} fetcher={(q) => api.get('/api/patients', { q, limit: 10 }).then((r) => r.items)} renderItem={(p) => `#${p.patient_id} ${p.name}${p.city ? ' · ' + p.city : ''}`} />}
        <SearchSelect label="Medicine" picked={medicine} onPick={(m) => { setMedicine(m); setRx(''); }} fetcher={(q) => api.get('/api/medicines', { q, limit: 10 }).then((r) => r.items)} renderItem={(m) => `${m.name} (${m.generic_name})${m.requires_prescription ? ' · Rx' : ''} · stock ${m.total_stock}`} />
        <div className="grid grid-cols-2 gap-3">
          <Input label="Quantity" type="number" min="1" max="1000" value={qty} onChange={(e) => setQty(e.target.value)} required />
          {staff ? <Select label="Urgency" value={urgency} onChange={(e) => setUrgency(e.target.value)} options={URGENCY} /> : <div className="text-xs text-slate-500">Self-service requests enter the queue as <b>Normal</b>. A pharmacist reviews and sets urgency.</div>}
        </div>
        {medicine?.requires_prescription && (
          <div className="rounded-lg border border-amber-200 bg-amber-50 p-3">
            <div className="mb-2 text-sm font-medium text-amber-800">This medicine needs a prescription.</div>
            <Select label="Prescription" value={rx} onChange={(e) => setRx(e.target.value)} placeholder="Select prescription…" options={usable.map((p) => ({ value: p.prescription_id, label: `#${p.prescription_id} · ${p.doctor_name} · ${p.issued_date}${p.verified ? ' · verified' : ' · awaiting verification'}` }))} />
            <button type="button" className="mt-2 text-xs text-brand-600 underline" disabled={!patientId} onClick={() => setAddRx(true)}>Add a prescription</button>
          </div>)}
        <Textarea label="Notes" value={notes} onChange={(e) => setNotes(e.target.value)} />
        <ErrorBox error={error} />
        <div className="flex justify-end gap-2"><button type="button" className="btn-ghost" onClick={onClose}>Cancel</button><button className="btn-primary" disabled={busy || !medicine || (staff && !patient)}>{busy ? 'Submitting…' : 'Submit request'}</button></div>
      </form>
      {addRx && <PrescriptionForm patientId={staff ? patientId : null} medicine={medicine} onClose={() => setAddRx(false)} onDone={(id) => { setAddRx(false); rxList.reload().then(() => setRx(String(id))); }} />}
    </Modal>
  );
}

function UrgencyForm({ request, onClose, onDone }) {
  const toast = useToast();
  const [urgency, setUrgency] = useState(request.urgency), [reason, setReason] = useState(''), [error, setError] = useState('');
  async function submit(e) {
    e.preventDefault(); setError('');
    try { await api.put(`/api/requests/${request.request_id}/urgency`, { urgency, reason }); toast.success('Urgency updated and recorded'); onDone(); } catch (err) { setError(err.message); }
  }
  return (
    <Modal title={`Change urgency · request #${request.request_id}`} onClose={onClose}>
      <form onSubmit={submit} className="space-y-3">
        <p className="text-sm text-slate-600">{request.patient_name} · {request.medicine} · currently <Badge value={request.urgency} /></p>
        <Select label="New urgency" value={urgency} onChange={(e) => setUrgency(e.target.value)} options={URGENCY} />
        <Textarea label="Reason (required, recorded in the audit trail)" value={reason} onChange={(e) => setReason(e.target.value)} required minLength={5} />
        <ErrorBox error={error} /><div className="flex justify-end gap-2"><button type="button" className="btn-ghost" onClick={onClose}>Cancel</button><button className="btn-primary" disabled={urgency === request.urgency}>Update urgency</button></div>
      </form>
    </Modal>
  );
}

function RequestDetail({ id, onClose }) {
  const r = useAsync(() => api.get('/api/requests/' + id), [id]);
  const d = r.data;
  return (
    <Modal title={'Request #' + id} onClose={onClose} wide>
      {r.loading && <Spinner />}<ErrorBox error={r.error} />
      {d && (
        <div className="space-y-4 text-sm">
          <div className="grid grid-cols-2 gap-3 md:grid-cols-4">
            {[['Patient', d.patient_name], ['Medicine', d.medicine], ['Quantity', d.quantity], ['Status', <Badge key="s" value={d.status} />], ['Urgency', <Badge key="u" value={d.urgency} />], ['Queue position', d.queue_position || '–'], ['Waiting', minutes(d.waiting_minutes)], ['Substitution', <Badge key="x" value={d.substitution_status} />]]
              .map(([k, v]) => <div key={k}><div className="label">{k}</div>{v}</div>)}
          </div>
          <section><h3 className="mb-1 font-semibold">Priority history</h3>
            <DataTable rows={d.priority_history} columns={[{ label: 'When', render: (h) => fmtDate(h.changed_at) }, { label: 'From', render: (h) => h.old_urgency || '–' }, { label: 'To', render: (h) => <Badge value={h.new_urgency} /> }, { label: 'By', render: (h) => h.changed_by || 'system' }, { label: 'Reason', key: 'reason' }]} /></section>
          <section><h3 className="mb-1 font-semibold">Transactions</h3>
            <DataTable rows={d.transactions} empty="No transactions yet." columns={[{ label: 'Txn', key: 'transaction_id' }, { label: 'Status', render: (t) => <Badge value={t.status} /> }, { label: 'Qty', key: 'quantity' }, { label: 'When', render: (t) => fmtDate(t.created_at) }, { label: 'Notes', key: 'notes' }]} /></section>
        </div>)}
    </Modal>
  );
}

function PrescriptionsTab() {
  const { user } = useAuth(); const toast = useToast();
  const [verified, setVerified] = useState('');
  const list = useAsync(() => api.get('/api/prescriptions', { verified }), [verified]);
  async function verify(p) { try { await api.post(`/api/prescriptions/${p.prescription_id}/verify`); toast.success('Prescription verified'); list.reload(); } catch (e) { toast.error(e.message); } }
  return (
    <div>
      <div className="mb-3 flex gap-2"><select className="input max-w-[12rem]" value={verified} onChange={(e) => setVerified(e.target.value)} aria-label="Verification filter"><option value="">All</option><option value="0">Awaiting verification</option><option value="1">Verified</option></select>
        <button className="btn-ghost" onClick={list.reload}><RefreshCw size={14} /></button></div>
      {list.loading && !list.data ? <Spinner /> : <ErrorBox error={list.error} onRetry={list.reload} />}
      {list.data && <DataTable rowKey="prescription_id" rows={list.data.items} empty="No prescriptions." columns={[
        { label: '#', key: 'prescription_id' }, { label: 'Patient', key: 'patient_name' }, { label: 'Medicine', key: 'medicine' }, { label: 'Qty', key: 'quantity' }, { label: 'Doctor', key: 'doctor_name' }, { label: 'Issued', key: 'issued_date' },
        { label: 'Status', render: (p) => (p.verified ? <Badge value="Approved">Verified</Badge> : <Badge value="Proposed">Awaiting verification</Badge>) },
        { label: '', render: (p) => (!p.verified && user.role === 'Pharmacist' ? <button className="btn-teal btn-sm" onClick={() => verify(p)}>Verify</button> : (!p.verified && user.role === 'Admin' ? <span className="text-xs text-slate-400">Pharmacist only</span> : null)) }]} />}
    </div>
  );
}

export default function Queue() {
  const { user } = useAuth(); const toast = useToast();
  const staff = user.role !== 'Patient';
  const [tab, setTab] = useState(staff ? 'queue' : 'mine');
  const [modal, setModal] = useState(null), [offset, setOffset] = useState(0), [status, setStatus] = useState('');
  const LIMIT = 15;
  const queue = useAsync(() => api.get('/api/queue'), []);
  const reqs = useAsync(() => api.get('/api/requests', { status, limit: LIMIT, offset }), [status, offset]);
  const reloadAll = () => { queue.reload(); reqs.reload(); };
  const pos = {}; ((queue.data && queue.data.queue) || []).forEach((x) => { pos[x.request_id] = x.queue_position; });
  async function cancel(r) {
    if (!window.confirm(`Cancel request #${r.request_id}?`)) return;
    try { await api.post(`/api/requests/${r.request_id}/cancel`); toast.success('Request cancelled'); reloadAll(); } catch (e) { toast.error(e.message); }
  }
  const actions = (r) => (
    <div className="flex gap-1">
      <button className="btn-ghost btn-sm" aria-label={'Details ' + r.request_id} onClick={() => setModal({ type: 'detail', id: r.request_id })}><Eye size={12} /></button>
      {staff && (r.status === 'Pending' || r.status === 'Waiting') && <button className="btn-ghost btn-sm" onClick={() => setModal({ type: 'urgency', r })}><ArrowUpDown size={12} /> Urgency</button>}
      {(r.status === 'Pending' || r.status === 'Waiting') && <button className="btn-ghost btn-sm" onClick={() => cancel(r)}>Cancel</button>}
    </div>);
  const tabs = staff ? [{ value: 'queue', label: 'Priority queue' }, { value: 'all', label: 'All requests' }, { value: 'rx', label: 'Prescriptions' }] : [{ value: 'mine', label: 'My requests' }, { value: 'rx', label: 'My prescriptions' }];
  return (
    <div>
      <PageHeader title={staff ? 'Priority Queue' : 'My Requests'} subtitle="Critical → High → Medium → Normal; within the same urgency the longest-waiting request goes first.">
        <button className="btn-ghost" onClick={reloadAll}><RefreshCw size={14} /> Refresh</button>
        <button className="btn-primary" onClick={() => setModal({ type: 'new' })}><Plus size={14} /> New request</button>
      </PageHeader>
      <Tabs tabs={tabs} value={tab} onChange={setTab} />
      {tab === 'queue' && (<>
        {queue.loading && !queue.data ? <Spinner /> : <ErrorBox error={queue.error} onRetry={queue.reload} />}
        {queue.data && <DataTable rowKey="request_id" rows={queue.data.queue} empty="The priority queue is empty." columns={[
          { label: 'Position', render: (r) => <span className="font-bold" data-testid="pos">{r.queue_position}</span> }, { label: 'Req', key: 'request_id' }, { label: 'Patient', key: 'patient_name' }, { label: 'Medicine', key: 'medicine' }, { label: 'Qty', key: 'quantity' },
          { label: 'Urgency', render: (r) => <Badge value={r.urgency} /> }, { label: 'Waiting', render: (r) => minutes(r.waiting_minutes) }, { label: 'In stock', render: (r) => (r.in_stock_now ? <Badge value="OK">Yes</Badge> : <Badge value="Out of stock">No</Badge>) }, { label: '', render: actions }]} />}
        {queue.data && queue.data.waiting_length > 0 && <p className="mt-3 text-sm text-slate-500">{queue.data.waiting_length} request(s) are on the waiting list (out of stock).</p>}
      </>)}
      {(tab === 'all' || tab === 'mine') && (<>
        <div className="mb-3"><select className="input max-w-[12rem]" value={status} onChange={(e) => { setStatus(e.target.value); setOffset(0); }} aria-label="Status filter"><option value="">All statuses</option>{['Pending', 'Waiting', 'Distributed', 'Rejected', 'Cancelled'].map((s) => <option key={s}>{s}</option>)}</select></div>
        {reqs.loading && !reqs.data ? <Spinner /> : <ErrorBox error={reqs.error} onRetry={reqs.reload} />}
        {reqs.data && (<>
          <DataTable rowKey="request_id" rows={reqs.data.items} empty="No requests found." columns={[
            { label: 'Req', key: 'request_id' }, ...(staff ? [{ label: 'Patient', key: 'patient_name' }] : []), { label: 'Medicine', key: 'medicine' }, { label: 'Qty', key: 'quantity' }, { label: 'Urgency', render: (r) => <Badge value={r.urgency} /> },
            { label: 'Status', render: (r) => <Badge value={r.status} /> }, { label: 'Queue pos.', render: (r) => pos[r.request_id] || '–' }, { label: 'Requested', render: (r) => fmtDate(r.requested_at) }, { label: '', render: actions }]} />
          <Pagination total={reqs.data.total} limit={LIMIT} offset={offset} onChange={setOffset} />
        </>)}
      </>)}
      {tab === 'rx' && <PrescriptionsTab />}
      {modal?.type === 'new' && <RequestForm onClose={() => setModal(null)} onDone={() => { setModal(null); reloadAll(); }} />}
      {modal?.type === 'urgency' && <UrgencyForm request={modal.r} onClose={() => setModal(null)} onDone={() => { setModal(null); reloadAll(); }} />}
      {modal?.type === 'detail' && <RequestDetail id={modal.id} onClose={() => setModal(null)} />}
    </div>
  );
}
