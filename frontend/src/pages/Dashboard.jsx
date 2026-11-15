// SHAURYA - role-aware dashboard (all figures come from the database through the API).
import React from 'react';
import { Link } from 'react-router-dom';
import { api } from '../api.js';
import { useAuth } from '../auth.jsx';
import { useAsync, PageHeader, StatCard, DataTable, Badge, ErrorBox, Spinner, fmtDate, minutes } from '../ui.jsx';
import { TrendChart, HBar, Donut } from './charts.jsx';

function StaffDashboard() {
  const sum = useAsync(() => api.get('/api/analytics/summary'), []);
  const trend = useAsync(() => api.get('/api/analytics/trends', { days: 14 }), []);
  const top = useAsync(() => api.get('/api/analytics/top-medicines', { limit: 8 }), []);
  const urg = useAsync(() => api.get('/api/analytics/urgency'), []);
  const st = useAsync(() => api.get('/api/analytics/status-breakdown'), []);
  const s = sum.data;
  return (
    <div>
      <PageHeader title="Dashboard" subtitle="Live overview of requests, stock and distribution" />
      {sum.loading && !s ? <Spinner /> : <ErrorBox error={sum.error} onRetry={sum.reload} />}
      {s && (
        <div className="mb-5 grid grid-cols-2 gap-3 md:grid-cols-4">
          <StatCard label="Patients" value={s.patients} /><StatCard label="Medicines" value={s.medicines} /><StatCard label="Pharmacies" value={s.pharmacies} /><StatCard label="Completed distributions" value={s.completed_distributions} tone="teal" hint={s.units_distributed + ' units'} />
          <StatCard label="Critical patients" value={s.critical_patients} tone="red" hint="open requests" /><StatCard label="Pending requests" value={s.pending_requests} /><StatCard label="Waiting list" value={s.waiting_requests} tone="amber" /><StatCard label="Unavailable medicines" value={s.unavailable_medicines} tone="amber" />
          <StatCard label="Low-stock items" value={s.low_stock_items} tone="amber" /><StatCard label="Expired batches" value={s.expired_batches} tone="red" hint={s.expired_units + ' units'} /><StatCard label="Near-expiry batches" value={s.near_expiry_batches} tone="amber" />
          <Link to="/distribution" className="card flex items-center justify-center bg-teal-600 text-sm font-semibold text-white hover:bg-teal-700">Go to distribution →</Link>
        </div>)}
      <div className="grid gap-4 lg:grid-cols-2">
        <TrendChart data={trend.data && trend.data.items} />
        <HBar title="Most requested medicines" subtitle="By generic name" data={top.data && top.data.items} nameKey="medicine" valueKey="requests" label="Requests" />
        <Donut title="Open requests by urgency" data={urg.data && Object.entries(urg.data.counts).map(([name, value]) => ({ name, value }))} />
        <Donut title="Requests by status" data={st.data && st.data.items.map((x) => ({ name: x.status, value: x.count }))} />
      </div>
    </div>
  );
}

function PatientDashboard() {
  const { user } = useAuth();
  const reqs = useAsync(() => api.get('/api/requests', { limit: 100 }), []);
  const queue = useAsync(() => api.get('/api/queue'), []);
  const notes = useAsync(() => api.get('/api/notifications', { limit: 5 }), []);
  const items = (reqs.data && reqs.data.items) || [];
  const count = (st) => items.filter((r) => r.status === st).length;
  const pos = {}; ((queue.data && queue.data.queue) || []).forEach((q) => { pos[q.request_id] = q.queue_position; });
  const open = items.filter((r) => r.status === 'Pending' || r.status === 'Waiting');
  return (
    <div>
      <PageHeader title={'Hello, ' + user.full_name} subtitle="Your medicine requests at a glance">
        <Link to="/queue" className="btn-primary">Request a medicine</Link><Link to="/pharmacies" className="btn-ghost">Find a pharmacy</Link>
      </PageHeader>
      {reqs.loading && !reqs.data ? <Spinner /> : <ErrorBox error={reqs.error} onRetry={reqs.reload} />}
      <div className="mb-5 grid grid-cols-2 gap-3 md:grid-cols-4">
        <StatCard label="Pending" value={count('Pending')} /><StatCard label="Waiting for stock" value={count('Waiting')} tone="amber" /><StatCard label="Received" value={count('Distributed')} tone="teal" /><StatCard label="Unread notifications" value={notes.data ? notes.data.unread : '–'} />
      </div>
      <h2 className="mb-2 font-semibold">Open requests</h2>
      <DataTable rowKey="request_id" rows={open} empty="You have no open requests." columns={[{ label: 'Req', key: 'request_id' }, { label: 'Medicine', key: 'medicine' }, { label: 'Qty', key: 'quantity' }, { label: 'Urgency', render: (r) => <Badge value={r.urgency} /> },
        { label: 'Status', render: (r) => <Badge value={r.status} /> }, { label: 'Queue position', render: (r) => pos[r.request_id] || '–' }, { label: 'Waiting', render: (r) => minutes(r.waiting_minutes) }]} />
      <h2 className="mb-2 mt-5 font-semibold">Recent notifications</h2>
      <DataTable rowKey="notification_id" rows={(notes.data && notes.data.items) || []} empty="No notifications." columns={[{ label: 'When', render: (n) => fmtDate(n.created_at) }, { label: 'Title', key: 'title' }, { label: 'Message', key: 'message' }]} />
    </div>
  );
}

export default function Dashboard() {
  const { user } = useAuth();
  return user.role === 'Patient' ? <PatientDashboard /> : <StaffDashboard />;
}
