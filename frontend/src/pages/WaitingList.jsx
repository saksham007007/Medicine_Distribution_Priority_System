// ANKIT - waiting list: pending requests in priority order, restock re-check, substitution decisions.
import React, { useState } from 'react';
import { RefreshCw } from 'lucide-react';
import { api } from '../api.js';
import { useAuth } from '../auth.jsx';
import { AlternativesPanel } from './Alternatives.jsx';
import { useAsync, PageHeader, DataTable, Badge, Modal, Textarea, ErrorBox, Spinner, Tabs, useToast, minutes, fmtDate } from '../ui.jsx';

function DecisionModal({ item, onClose, onDone }) {
  const toast = useToast(); const [note, setNote] = useState(''), [error, setError] = useState('');
  async function decide(approve) {
    setError('');
    try { await api.post(`/api/requests/${item.request_id}/substitution/decision`, { approve, note }); toast.success(approve ? 'Alternative approved' : 'Alternative rejected'); onDone(); } catch (err) { setError(err.message); }
  }
  return (
    <Modal title={`Substitution decision · request #${item.request_id}`} onClose={onClose}>
      <div className="space-y-3 text-sm"><p>{item.patient_name} requested <b>{item.medicine}</b>. Proposed alternative: <b>{item.proposed_substitute}</b>.</p>
        <p className="text-xs text-slate-500">Confirm prescription requirements and clinical suitability. A note is required when the alternative differs in strength.</p>
        <Textarea label="Clinical note" value={note} onChange={(e) => setNote(e.target.value)} /><ErrorBox error={error} />
        <div className="flex justify-end gap-2"><button className="btn-ghost" onClick={() => decide(false)}>Reject</button><button className="btn-teal" onClick={() => decide(true)} data-testid="approve-sub">Approve alternative</button></div></div>
    </Modal>
  );
}

export default function WaitingList() {
  const { user } = useAuth(); const toast = useToast();
  const staff = user.role !== 'Patient', pharmacist = user.role === 'Pharmacist';
  const [status, setStatus] = useState('Waiting'), [modal, setModal] = useState(null);
  const list = useAsync(() => api.get('/api/waiting-list', { status }), [status]);
  async function recheck() {
    try { const r = await api.post('/api/waiting-list/process'); toast.success(r.promoted ? `${r.promoted} request(s) returned to the priority queue` : 'No waiting request can be served with current stock'); list.reload(); } catch (e) { toast.error(e.message); }
  }
  return (
    <div>
      <PageHeader title="Waiting List" subtitle="Requests waiting for stock keep their priority and return to the queue automatically after restocking.">
        <button className="btn-ghost" onClick={list.reload}><RefreshCw size={14} /> Refresh</button>
        {staff && <button className="btn-primary" onClick={recheck}>Re-check stock now</button>}
      </PageHeader>
      <Tabs value={status} onChange={setStatus} tabs={[{ value: 'Waiting', label: 'Waiting' }, { value: 'Completed', label: 'Completed' }, { value: 'Cancelled', label: 'Cancelled' }]} />
      {list.loading && !list.data ? <Spinner /> : <ErrorBox error={list.error} onRetry={list.reload} />}
      {list.data && <DataTable rowKey="waiting_id" rows={list.data.items} empty={status === 'Waiting' ? 'The waiting list is empty.' : 'Nothing here.'} columns={[
        { label: 'Req', key: 'request_id' }, { label: 'Patient', key: 'patient_name' }, { label: 'Medicine', key: 'medicine' }, { label: 'Qty', key: 'quantity' }, { label: 'Urgency', render: (w) => <Badge value={w.urgency} /> },
        { label: 'Waiting', render: (w) => minutes(w.waiting_minutes) }, { label: 'Since', render: (w) => fmtDate(w.added_at) },
        { label: 'Substitution', render: (w) => (w.substitution_status === 'None' ? '–' : <span><Badge value={w.substitution_status} /> {w.proposed_substitute}</span>) },
        ...(staff ? [{ label: '', render: (w) => w.status !== 'Waiting' ? null : (
          <div className="flex gap-1"><button className="btn-ghost btn-sm" onClick={() => setModal({ type: 'alts', w })}>Alternatives</button>
            {pharmacist && w.substitution_status === 'Proposed' && <button className="btn-teal btn-sm" onClick={() => setModal({ type: 'decide', w })}>Decide</button>}</div>) }] : [])]} />}
      {modal?.type === 'alts' && <Modal title={'Alternatives · ' + modal.w.medicine} onClose={() => setModal(null)} wide><AlternativesPanel medicineId={modal.w.medicine_id} onChanged={list.reload} /></Modal>}
      {modal?.type === 'decide' && <DecisionModal item={modal.w} onClose={() => setModal(null)} onDone={() => { setModal(null); list.reload(); }} />}
    </div>
  );
}
