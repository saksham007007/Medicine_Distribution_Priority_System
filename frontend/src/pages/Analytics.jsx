// SHAURYA - analytics and reports.
import React, { useState } from 'react';
import { Download, RefreshCw } from 'lucide-react';
import { api } from '../api.js';
import { useAsync, PageHeader, StatCard, DataTable, ErrorBox, Spinner, useToast } from '../ui.jsx';
import { TrendChart, HBar, Donut } from './charts.jsx';

export default function Analytics() {
  const toast = useToast();
  const [days, setDays] = useState(30), [topDays, setTopDays] = useState(0);
  const sum = useAsync(() => api.get('/api/analytics/summary'), []);
  const trend = useAsync(() => api.get('/api/analytics/trends', { days }), [days]);
  const top = useAsync(() => api.get('/api/analytics/top-medicines', { limit: 10, days: topDays }), [topDays]);
  const cat = useAsync(() => api.get('/api/analytics/category-stock'), []);
  const ph = useAsync(() => api.get('/api/analytics/pharmacy-inventory'), []);
  const urg = useAsync(() => api.get('/api/analytics/urgency'), []);
  const s = sum.data;
  const reloadAll = () => [sum, trend, top, cat, ph, urg].forEach((x) => x.reload());
  async function exportCsv() { try { await api.download('/api/reports/transactions.csv', 'transactions.csv'); toast.success('Report downloaded'); } catch (e) { toast.error(e.message); } }
  return (
    <div>
      <PageHeader title="Analytics" subtitle="Computed from the live database (sample demonstration data)">
        <select className="input w-auto" value={days} onChange={(e) => setDays(Number(e.target.value))} aria-label="Trend window"><option value={7}>Last 7 days</option><option value={14}>Last 14 days</option><option value={30}>Last 30 days</option><option value={90}>Last 90 days</option></select>
        <button className="btn-ghost" onClick={reloadAll}><RefreshCw size={14} /> Refresh</button><button className="btn-primary" onClick={exportCsv}><Download size={14} /> Transactions CSV</button>
      </PageHeader>
      {sum.loading && !s ? <Spinner /> : <ErrorBox error={sum.error} onRetry={sum.reload} />}
      {s && <div className="mb-5 grid grid-cols-2 gap-3 md:grid-cols-5"><StatCard label="Completed distributions" value={s.completed_distributions} tone="teal" /><StatCard label="Units distributed" value={s.units_distributed} tone="teal" /><StatCard label="Pending requests" value={s.pending_requests} /><StatCard label="Low-stock items" value={s.low_stock_items} tone="amber" /><StatCard label="Expired units" value={s.expired_units} tone="red" /></div>}
      <div className="grid gap-4 lg:grid-cols-2">
        <TrendChart data={trend.data && trend.data.items} />
        <div>
          <div className="mb-2 flex justify-end"><select className="input w-auto" value={topDays} onChange={(e) => setTopDays(Number(e.target.value))} aria-label="Top medicines window"><option value={0}>All time</option><option value={7}>Last 7 days</option><option value={30}>Last 30 days</option></select></div>
          <HBar title="Most requested medicines" subtitle="By generic name" data={top.data && top.data.items} nameKey="medicine" valueKey="requests" label="Requests" />
        </div>
        <HBar title="Stock by category" subtitle="Unexpired units across all pharmacies" data={cat.data && cat.data.items} nameKey="category" valueKey="units" color="#0d9488" label="Units" />
        <Donut title="Open requests by urgency" data={urg.data && Object.entries(urg.data.counts).map(([name, value]) => ({ name, value }))} />
      </div>
      <h2 className="mb-2 mt-6 font-semibold">Pharmacy-wise inventory</h2>
      {ph.data && <DataTable rowKey="pharmacy_id" rows={ph.data.items} columns={[{ label: 'Pharmacy', key: 'pharmacy' }, { label: 'City', key: 'city' }, { label: 'Status', key: 'status' }, { label: 'Distinct medicines', key: 'medicines' }, { label: 'Units in stock', key: 'units' }, { label: 'Expired units', key: 'expired_units' }]} />}
      <h2 className="mb-2 mt-6 font-semibold">Demand by medicine (open requests)</h2>
      {urg.data && <DataTable rows={urg.data.demand} empty="No open requests." columns={[{ label: 'Medicine', key: 'medicine' }, { label: 'Open requests', key: 'requests' }]} />}
    </div>
  );
}
