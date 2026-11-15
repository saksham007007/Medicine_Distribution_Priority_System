// ANKIT - smart distribution workflow: evaluate head of queue -> recommend pharmacies -> authorised distribution.
import React, { useState } from 'react';
import { Truck, RefreshCw, ShieldAlert } from 'lucide-react';
import { api } from '../api.js';
import { useAuth } from '../auth.jsx';
import { useAsync, PageHeader, DataTable, Badge, Modal, Textarea, ErrorBox, Spinner, useToast, minutes } from '../ui.jsx';

function RejectModal({ request, onClose, onDone }) {
  const toast = useToast(); const [reason, setReason] = useState(''), [error, setError] = useState('');
  async function submit(e) { e.preventDefault(); setError(''); try { await api.post('/api/distribution/reject', { request_id: request.request_id, reason }); toast.success('Request rejected and patient notified'); onDone(); } catch (err) { setError(err.message); } }
  return (
    <Modal title={`Reject request #${request.request_id}`} onClose={onClose}>
      <form onSubmit={submit} className="space-y-3"><p className="text-sm text-slate-600">{request.patient_name} · {request.medicine}</p>
        <Textarea label="Reason (shown to the patient)" value={reason} onChange={(e) => setReason(e.target.value)} required minLength={5} /><ErrorBox error={error} />
        <div className="flex justify-end gap-2"><button type="button" className="btn-ghost" onClick={onClose}>Cancel</button><button className="btn-danger">Reject request</button></div></form>
    </Modal>
  );
}

function Recommendations({ details, headRequestId, onDistribute, busy }) {
  return (
    <DataTable rowKey="pharmacy_id" rows={details.recommendations} empty="No pharmacy has sufficient stock." columns={[
      { label: 'Pharmacy', render: (p) => <span className="font-medium">{p.name}</span> }, { label: 'City', key: 'city' }, { label: 'Units available', key: 'available' },
      { label: 'Distance', render: (p) => (p.distance_km === null ? '–' : p.distance_km + ' km') }, { label: 'Earliest expiry', key: 'earliest_expiry' }, { label: 'Phone', key: 'phone' },
      { label: '', render: (p) => <button className="btn-teal btn-sm" disabled={busy || details.request_id !== headRequestId} data-testid={'distribute-' + p.pharmacy_id} onClick={() => onDistribute(p)}><Truck size={12} /> Distribute here</button> }]} />
  );
}

export default function Distribution() {
  const { user } = useAuth(); const toast = useToast();
  const [result, setResult] = useState(null), [busy, setBusy] = useState(false), [error, setError] = useState(''), [modal, setModal] = useState(null);
  const queue = useAsync(() => api.get('/api/queue'), []);
  const head = queue.data && queue.data.queue[0];

  async function evaluate() {
    setBusy(true); setError(''); setResult(null);
    try { const r = await api.post('/api/distribution/next'); setResult(r); queue.reload(); } catch (e) { setError(e.message); } finally { setBusy(false); }
  }
  async function distribute(p) {
    setBusy(true); setError('');
    try {
      const r = await api.post('/api/distribution/distribute', { request_id: result.request.request_id, pharmacy_id: p.pharmacy_id });
      toast.success(`Distributed (transaction #${r.transaction_id}). ${r.remaining_stock_at_pharmacy} unit(s) left at ${p.name}.` + (r.low_stock_alert ? ' Low-stock alert raised.' : ''));
      setResult(null); queue.reload();
    } catch (e) { setError(e.message); } finally { setBusy(false); }
  }
  async function verifyRx() {
    try { const rq = await api.get('/api/requests/' + result.request.request_id); await api.post(`/api/prescriptions/${rq.prescription_id}/verify`); toast.success('Prescription verified'); await evaluate(); } catch (e) { setError(e.message); }
  }
  const act = result && result.action;
  return (
    <div>
      <PageHeader title="Distribution" subtitle="Requests are always served in priority order. The system recommends pharmacies with enough unexpired stock.">
        <button className="btn-ghost" onClick={queue.reload}><RefreshCw size={14} /> Refresh</button>
        <button className="btn-primary" onClick={evaluate} disabled={busy} data-testid="evaluate-next"><Truck size={14} /> Evaluate next request</button>
      </PageHeader>
      <ErrorBox error={error} />
      <div className="grid gap-4 lg:grid-cols-3">
        <div className="lg:col-span-2 space-y-3">
          {!result && <div className="card text-sm text-slate-500">{head ? <>Next in queue: <b>#{head.request_id} {head.patient_name}</b> — {head.medicine} ×{head.quantity} <Badge value={head.urgency} />. Press “Evaluate next request” to check stock and get pharmacy recommendations.</> : 'The priority queue is empty.'}</div>}
          {result && (
            <div className="card space-y-3" data-testid="result" data-action={act}>
              <div className="flex flex-wrap items-center justify-between gap-2">
                <div><div className="text-xs font-semibold uppercase text-slate-500">Request #{result.request ? result.request.request_id : ''}</div>
                  {result.request && <div className="text-lg font-bold">{result.request.patient_name} <Badge value={result.request.urgency} /></div>}
                  {result.request && <div className="text-sm text-slate-600">{result.details.medicine.name} ({result.details.medicine.generic_name}) ×{result.request.quantity}</div>}</div>
                <Badge value={act === 'ready' ? 'OK' : act === 'queue_empty' ? 'None' : 'Waiting'}>{act === 'ready' ? 'Ready to distribute' : act === 'blocked_prescription' ? 'Prescription needed' : act === 'added_to_waiting_list' ? 'Moved to waiting list' : 'Queue empty'}</Badge>
              </div>
              <p className="text-sm">{result.message}</p>
              {act === 'ready' && <Recommendations details={result.details} headRequestId={result.request.request_id} busy={busy} onDistribute={distribute} />}
              {act === 'blocked_prescription' && (
                <div className="flex flex-wrap items-center gap-2 rounded-lg border border-amber-200 bg-amber-50 p-3 text-sm text-amber-800"><ShieldAlert size={16} /> A pharmacist must verify the prescription.
                  {user.role === 'Pharmacist' ? <button className="btn-teal btn-sm" onClick={verifyRx} data-testid="verify-rx">Verify prescription</button> : <span className="text-xs">(Only the Pharmacist role can verify.)</span>}
                  <button className="btn-danger btn-sm" onClick={() => setModal(result.request)}>Reject request</button></div>)}
              {act === 'added_to_waiting_list' && result.details.alternatives && (
                <div><h3 className="mb-1 text-sm font-semibold">Possible alternatives (pharmacist approval required)</h3>
                  <DataTable rowKey="medicine_id" rows={result.details.alternatives.slice(0, 6)} columns={[{ label: 'Alternative', key: 'name' }, { label: 'Match', key: 'match_type' }, { label: 'Available units', key: 'available_units' }]} />
                  <p className="mt-1 text-xs text-slate-500">Open “Waiting List” to propose one for this request.</p></div>)}
              {act === 'ready' && <div className="flex justify-end"><button className="btn-ghost btn-sm" onClick={() => setModal(result.request)}>Reject this request</button></div>}
            </div>)}
        </div>
        <div className="card">
          <h2 className="mb-2 font-semibold">Queue preview</h2>
          {queue.loading && !queue.data ? <Spinner /> : <ErrorBox error={queue.error} />}
          {queue.data && queue.data.queue.length === 0 && <p className="text-sm text-slate-500">Nothing waiting.</p>}
          <ol className="space-y-2 text-sm">{queue.data && queue.data.queue.slice(0, 8).map((r) => (
            <li key={r.request_id} className="flex items-center justify-between gap-2 rounded-lg bg-slate-50 px-3 py-2"><span><b>{r.queue_position}.</b> {r.patient_name}<span className="block text-xs text-slate-500">{r.medicine} ×{r.quantity} · {minutes(r.waiting_minutes)}</span></span><Badge value={r.urgency} /></li>))}</ol>
        </div>
      </div>
      {modal && <RejectModal request={modal} onClose={() => setModal(null)} onDone={() => { setModal(null); setResult(null); queue.reload(); }} />}
    </div>
  );
}
