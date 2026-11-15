// ANKIT - distribution transaction history with search, filters and CSV export.
import React, { useState } from 'react';
import { Download, Eye } from 'lucide-react';
import { api } from '../api.js';
import { useAuth } from '../auth.jsx';
import { useAsync, useDebounced, PageHeader, DataTable, Pagination, Badge, Modal, ErrorBox, Spinner, useToast, fmtDate } from '../ui.jsx';

const LIMIT = 20;

function Detail({ id, onClose }) {
  const t = useAsync(() => api.get('/api/transactions/' + id), [id]);
  const d = t.data;
  return (
    <Modal title={'Transaction #' + id} onClose={onClose}>
      {t.loading && <Spinner />}<ErrorBox error={t.error} />
      {d && (<div className="space-y-3 text-sm">
        <div className="grid grid-cols-2 gap-3">{[['Patient', d.patient_name], ['Medicine', d.medicine_name], ['Quantity', d.quantity], ['Status', <Badge key="s" value={d.status} />], ['Pharmacy', d.pharmacy || '–'], ['When', fmtDate(d.created_at)], ['Request', '#' + d.request_id], ['Notes', d.notes || '–']].map(([k, v]) => <div key={k}><div className="label">{k}</div>{v}</div>)}</div>
        <section><h3 className="mb-1 font-semibold">Batches used (first-expiry-first-out)</h3><DataTable rows={d.batches} empty="No batch detail (sample history or non-distribution record)." columns={[{ label: 'Batch', key: 'batch_number' }, { label: 'Expiry', key: 'expiry_date' }, { label: 'Units', key: 'quantity' }]} /></section>
      </div>)}
    </Modal>
  );
}

export default function Transactions() {
  const { user } = useAuth(); const toast = useToast();
  const staff = user.role !== 'Patient';
  const [f, setF] = useState({ q: '', status: '', from: '', to: '', pharmacy_id: '' }), [offset, setOffset] = useState(0), [detail, setDetail] = useState(null);
  const dq = useDebounced(f.q);
  const pharmacies = useAsync(() => (staff ? api.get('/api/pharmacies') : Promise.resolve({ items: [] })), [staff]);
  const list = useAsync(() => api.get('/api/transactions', { q: dq, status: f.status, from: f.from, to: f.to, pharmacy_id: f.pharmacy_id, limit: LIMIT, offset }), [dq, f.status, f.from, f.to, f.pharmacy_id, offset]);
  const set = (k) => (e) => { setF({ ...f, [k]: e.target.value }); setOffset(0); };
  async function exportCsv() { try { await api.download('/api/reports/transactions.csv', 'transactions.csv'); toast.success('CSV downloaded'); } catch (e) { toast.error(e.message); } }
  return (
    <div>
      <PageHeader title={staff ? 'Transactions' : 'My Transactions'} subtitle="Complete distribution history">
        {staff && <button className="btn-ghost" onClick={exportCsv}><Download size={14} /> Export CSV</button>}
      </PageHeader>
      <div className="card mb-3 grid gap-3 md:grid-cols-5">
        <input className="input md:col-span-2" placeholder={staff ? 'Search patient or medicine' : 'Search medicine'} value={f.q} onChange={set('q')} aria-label="Search transactions" />
        <select className="input" value={f.status} onChange={set('status')} aria-label="Status"><option value="">All statuses</option><option>Distributed</option><option>Waiting</option><option>Rejected</option></select>
        <input className="input" type="date" value={f.from} onChange={set('from')} aria-label="From date" /><input className="input" type="date" value={f.to} onChange={set('to')} aria-label="To date" />
        {staff && <select className="input md:col-span-2" value={f.pharmacy_id} onChange={set('pharmacy_id')} aria-label="Pharmacy"><option value="">All pharmacies</option>{((pharmacies.data && pharmacies.data.items) || []).map((p) => <option key={p.pharmacy_id} value={p.pharmacy_id}>{p.name}</option>)}</select>}
      </div>
      {list.loading && !list.data ? <Spinner /> : <ErrorBox error={list.error} onRetry={list.reload} />}
      {list.data && (<>
        <DataTable rowKey="transaction_id" rows={list.data.items} empty="No transactions found." columns={[
          { label: 'Txn', key: 'transaction_id' }, { label: 'When', render: (t) => fmtDate(t.created_at) }, ...(staff ? [{ label: 'Patient', key: 'patient_name' }] : []), { label: 'Medicine', key: 'medicine_name' }, { label: 'Qty', key: 'quantity' },
          { label: 'Pharmacy', render: (t) => t.pharmacy || '–' }, { label: 'Status', render: (t) => <Badge value={t.status} /> }, { label: '', render: (t) => <button className="btn-ghost btn-sm" aria-label={'Transaction ' + t.transaction_id} onClick={() => setDetail(t.transaction_id)}><Eye size={12} /></button> }]} />
        <Pagination total={list.data.total} limit={LIMIT} offset={offset} onChange={setOffset} />
      </>)}
      {detail && <Detail id={detail} onClose={() => setDetail(null)} />}
    </div>
  );
}
