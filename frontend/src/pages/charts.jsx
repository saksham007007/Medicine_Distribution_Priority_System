// SHAURYA - reusable chart cards (Recharts).
import React from 'react';
import { ResponsiveContainer, LineChart, Line, BarChart, Bar, PieChart, Pie, Cell, XAxis, YAxis, Tooltip, CartesianGrid, Legend } from 'recharts';

export const COLORS = ['#2563eb', '#0d9488', '#f59e0b', '#ef4444', '#8b5cf6', '#64748b', '#14b8a6', '#f97316'];
const URGENCY_COLORS = { Critical: '#ef4444', High: '#f97316', Medium: '#f59e0b', Normal: '#64748b' };

export function ChartCard({ title, subtitle, empty, children }) {
  return (
    <div className="card" data-testid={'chart-' + title.toLowerCase().replace(/[^a-z0-9]+/g, '-')}>
      <h3 className="font-semibold text-slate-800">{title}</h3>{subtitle && <p className="mb-2 text-xs text-slate-500">{subtitle}</p>}
      {empty ? <div className="flex h-64 items-center justify-center text-sm text-slate-400">No data yet</div> : <div className="h-64 w-full">{children}</div>}
    </div>
  );
}

export function TrendChart({ data }) {
  return (
    <ChartCard title="Distribution trend" subtitle="Completed distributions per day" empty={!data || data.length === 0}>
      <ResponsiveContainer width="100%" height="100%"><LineChart data={data} margin={{ left: -10, right: 10 }}><CartesianGrid strokeDasharray="3 3" stroke="#e2e8f0" />
        <XAxis dataKey="date" tick={{ fontSize: 10 }} tickFormatter={(d) => String(d).slice(5)} /><YAxis allowDecimals={false} tick={{ fontSize: 11 }} /><Tooltip /><Legend />
        <Line type="monotone" dataKey="distributions" name="Distributions" stroke="#2563eb" strokeWidth={2} dot={false} /><Line type="monotone" dataKey="units" name="Units" stroke="#0d9488" strokeWidth={2} dot={false} /></LineChart></ResponsiveContainer>
    </ChartCard>
  );
}

export function HBar({ title, subtitle, data, nameKey, valueKey, color = COLORS[0], label }) {
  return (
    <ChartCard title={title} subtitle={subtitle} empty={!data || data.length === 0}>
      <ResponsiveContainer width="100%" height="100%"><BarChart data={data} layout="vertical" margin={{ left: 10, right: 20 }}><CartesianGrid strokeDasharray="3 3" stroke="#e2e8f0" />
        <XAxis type="number" allowDecimals={false} tick={{ fontSize: 11 }} /><YAxis type="category" dataKey={nameKey} width={120} tick={{ fontSize: 10 }} /><Tooltip />
        <Bar dataKey={valueKey} name={label || valueKey} fill={color} radius={[0, 4, 4, 0]} /></BarChart></ResponsiveContainer>
    </ChartCard>
  );
}

export function Donut({ title, subtitle, data }) {
  const rows = (data || []).filter((d) => d.value > 0);
  return (
    <ChartCard title={title} subtitle={subtitle} empty={rows.length === 0}>
      <ResponsiveContainer width="100%" height="100%"><PieChart><Pie data={rows} dataKey="value" nameKey="name" innerRadius={45} outerRadius={80} paddingAngle={2} label>
        {rows.map((d, i) => <Cell key={d.name} fill={URGENCY_COLORS[d.name] || COLORS[i % COLORS.length]} />)}</Pie><Tooltip /><Legend /></PieChart></ResponsiveContainer>
    </ChartCard>
  );
}
