// SAKSHAM - password change for everyone; system settings + audit log for Admin.
import React, { useState, useEffect } from 'react';
import { api } from '../api.js';
import { useAuth } from '../auth.jsx';
import { useAsync, PageHeader, DataTable, Input, Textarea, ErrorBox, Spinner, useToast, fmtDate, Tabs, useDebounced } from '../ui.jsx';

function PasswordCard() {
  const { logout } = useAuth(); const toast = useToast();
  const [f, setF] = useState({ current_password: '', new_password: '' }), [error, setError] = useState('');
  async function submit(e) {
    e.preventDefault(); setError('');
    try { await api.post('/api/auth/change-password', f); toast.success('Password changed. Please log in again.'); await logout(false); }
    catch (err) { setError(err.message); }
  }
  return (
    <form onSubmit={submit} className="card max-w-md space-y-3">
      <h2 className="font-semibold">Change password</h2>
      <Input label="Current password" type="password" value={f.current_password} onChange={(e) => setF({ ...f, current_password: e.target.value })} required />
      <Input label="New password" type="password" value={f.new_password} onChange={(e) => setF({ ...f, new_password: e.target.value })} required hint="8+ characters with a letter and a digit. All sessions are signed out." />
      <ErrorBox error={error} /><button className="btn-primary">Change password</button>
    </form>
  );
}

function SystemSettings() {
  const toast = useToast();
  const s = useAsync(() => api.get('/api/settings'), []);
  const [f, setF] = useState(null), [error, setError] = useState('');
  useEffect(() => { if (s.data) { const m = {}; s.data.items.forEach((i) => { m[i.key] = i.value; }); setF(m); } }, [s.data]);
  if (s.loading && !f) return <Spinner />;
  if (!f) return <ErrorBox error={s.error} onRetry={s.reload} />;
  async function save(e) {
    e.preventDefault(); setError('');
    try {
      await api.put('/api/settings', { near_expiry_days: Number(f.near_expiry_days), session_hours: Number(f.session_hours), default_reorder_level: Number(f.default_reorder_level), enforce_priority: f.enforce_priority === '1', data_notice: f.data_notice });
      toast.success('Settings saved'); s.reload();
    } catch (err) { setError(err.message); }
  }
  return (
    <form onSubmit={save} className="card max-w-xl space-y-3">
      <h2 className="font-semibold">System settings</h2>
      <Input label="Near-expiry window (days)" type="number" min="1" max="730" value={f.near_expiry_days} onChange={(e) => setF({ ...f, near_expiry_days: e.target.value })} />
      <Input label="Session lifetime (hours)" type="number" min="1" max="168" value={f.session_hours} onChange={(e) => setF({ ...f, session_hours: e.target.value })} />
      <Input label="Default reorder level" type="number" min="0" value={f.default_reorder_level} onChange={(e) => setF({ ...f, default_reorder_level: e.target.value })} />
      <label className="flex items-center gap-2 text-sm"><input type="checkbox" checked={f.enforce_priority === '1'} onChange={(e) => setF({ ...f, enforce_priority: e.target.checked ? '1' : '0' })} /> Enforce strict priority order (only the head of the queue may be distributed)</label>
      <Textarea label="Data notice banner" value={f.data_notice} onChange={(e) => setF({ ...f, data_notice: e.target.value })} />
      <ErrorBox error={error} /><button className="btn-primary" data-testid="save-settings">Save settings</button>
    </form>
  );
}

function AuditLog() {
  const [q, setQ] = useState(''); const dq = useDebounced(q);
  const logs = useAsync(() => api.get('/api/audit-logs', { q: dq, limit: 100 }), [dq]);
  return (
    <div>
      <input className="input mb-3 max-w-xs" placeholder="Filter (action, user, entity…)" value={q} onChange={(e) => setQ(e.target.value)} aria-label="Filter audit log" />
      {logs.loading && !logs.data ? <Spinner /> : <ErrorBox error={logs.error} onRetry={logs.reload} />}
      {logs.data && <DataTable rowKey="log_id" rows={logs.data.items} columns={[{ label: 'When', render: (l) => fmtDate(l.created_at) }, { label: 'User', key: 'username' }, { label: 'Action', key: 'action' }, { label: 'Entity', render: (l) => (l.entity || '') + (l.entity_id ? ' #' + l.entity_id : '') }, { label: 'Details', key: 'details' }]} />}
    </div>
  );
}

export default function SettingsPage() {
  const { user } = useAuth();
  const [tab, setTab] = useState('account');
  const tabs = [{ value: 'account', label: 'Account' }, ...(user.role === 'Admin' ? [{ value: 'system', label: 'System settings' }, { value: 'audit', label: 'Audit log' }] : [])];
  return (
    <div>
      <PageHeader title="Settings" subtitle={user.full_name + ' · ' + user.role + ' · @' + user.username} />
      <Tabs tabs={tabs} value={tab} onChange={setTab} />
      {tab === 'account' && <PasswordCard />}
      {tab === 'system' && <SystemSettings />}
      {tab === 'audit' && <AuditLog />}
    </div>
  );
}
