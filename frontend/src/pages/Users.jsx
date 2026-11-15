// SAKSHAM - user management (Admin).
import React, { useState } from 'react';
import { Plus, Pencil, Trash2, RefreshCw } from 'lucide-react';
import { api } from '../api.js';
import { useAsync, PageHeader, DataTable, Badge, Modal, Input, Select, ErrorBox, Spinner, useToast, fmtDate } from '../ui.jsx';

function UserForm({ user, pharmacies, onDone, onClose }) {
  const toast = useToast();
  const [f, setF] = useState(user ? { full_name: user.full_name, email: user.email || '', role: user.role, active: !!user.active, pharmacy_id: user.pharmacy_id || '', password: '' }
    : { username: '', password: '', role: 'Pharmacist', full_name: '', email: '', patient_id: '', pharmacy_id: '' });
  const [error, setError] = useState(''), [busy, setBusy] = useState(false);
  const set = (k) => (e) => setF({ ...f, [k]: e.target.type === 'checkbox' ? e.target.checked : e.target.value });
  async function submit(e) {
    e.preventDefault(); setBusy(true); setError('');
    try {
      const body = { ...f };
      if (user) { body.pharmacy_id = body.pharmacy_id ? Number(body.pharmacy_id) : 0; if (!body.password) delete body.password; await api.put('/api/users/' + user.user_id, body); toast.success('User updated'); }
      else { ['patient_id', 'pharmacy_id'].forEach((k) => { if (body[k] === '') delete body[k]; else body[k] = Number(body[k]); }); if (!body.email) delete body.email; await api.post('/api/users', body); toast.success('User created'); }
      onDone();
    } catch (err) { setError(err.message); } finally { setBusy(false); }
  }
  return (
    <Modal title={user ? 'Edit user ' + user.username : 'New user'} onClose={onClose}>
      <form onSubmit={submit} className="space-y-3">
        {!user && <Input label="Username" value={f.username} onChange={set('username')} required />}
        <Input label="Full name" value={f.full_name} onChange={set('full_name')} required />
        <Input label="Email" type="email" value={f.email} onChange={set('email')} />
        <Select label="Role" value={f.role} onChange={set('role')} options={['Admin', 'Pharmacist', 'Patient']} />
        {f.role === 'Patient' && !user && <Input label="Patient ID" type="number" value={f.patient_id} onChange={set('patient_id')} required hint="Existing patient record to link" />}
        {f.role === 'Pharmacist' && <Select label="Restrict to pharmacy" value={f.pharmacy_id} onChange={set('pharmacy_id')} placeholder="All pharmacies" options={pharmacies.map((p) => ({ value: p.pharmacy_id, label: p.name + ' (' + p.city + ')' }))} />}
        <Input label={user ? 'New password (optional)' : 'Password'} type="password" value={f.password} onChange={set('password')} required={!user} hint="8+ characters with a letter and a digit" />
        {user && <label className="flex items-center gap-2 text-sm"><input type="checkbox" checked={f.active} onChange={set('active')} /> Account active</label>}
        <ErrorBox error={error} />
        <div className="flex justify-end gap-2"><button type="button" className="btn-ghost" onClick={onClose}>Cancel</button><button className="btn-primary" disabled={busy}>{busy ? 'Saving…' : 'Save'}</button></div>
      </form>
    </Modal>
  );
}

export default function UsersPage() {
  const toast = useToast();
  const [role, setRole] = useState(''), [q, setQ] = useState(''), [modal, setModal] = useState(null);
  const users = useAsync(() => api.get('/api/users', { role, q }), [role, q]);
  const pharmacies = useAsync(() => api.get('/api/pharmacies'), []);
  async function deactivate(u) {
    if (!window.confirm('Deactivate ' + u.username + '? Their history is kept.')) return;
    try { await api.del('/api/users/' + u.user_id); toast.success('User deactivated'); users.reload(); } catch (e) { toast.error(e.message); }
  }
  return (
    <div>
      <PageHeader title="User Management" subtitle="Create accounts, assign roles and restrict pharmacists to a pharmacy.">
        <button className="btn-ghost" onClick={users.reload}><RefreshCw size={14} /> Refresh</button>
        <button className="btn-primary" onClick={() => setModal({})}><Plus size={14} /> New user</button>
      </PageHeader>
      <div className="mb-3 flex flex-wrap gap-2">
        <input className="input max-w-xs" placeholder="Search username or name" value={q} onChange={(e) => setQ(e.target.value)} aria-label="Search users" />
        <select className="input max-w-[10rem]" value={role} onChange={(e) => setRole(e.target.value)} aria-label="Filter role"><option value="">All roles</option><option>Admin</option><option>Pharmacist</option><option>Patient</option></select>
      </div>
      {users.loading && !users.data ? <Spinner /> : <ErrorBox error={users.error} onRetry={users.reload} />}
      {users.data && <DataTable rowKey="user_id" rows={users.data.items} columns={[
        { label: 'Username', key: 'username' }, { label: 'Name', key: 'full_name' }, { label: 'Role', render: (u) => <Badge value={u.role} /> },
        { label: 'Pharmacy', render: (u) => u.pharmacy_id || (u.role === 'Pharmacist' ? 'All' : '') }, { label: 'Patient', render: (u) => u.patient_id || '' },
        { label: 'Status', render: (u) => <Badge value={u.active ? 'Active' : 'Discontinued'}>{u.active ? 'Active' : 'Inactive'}</Badge> }, { label: 'Created', render: (u) => fmtDate(u.created_at) },
        { label: '', render: (u) => <div className="flex gap-1"><button className="btn-ghost btn-sm" aria-label={'Edit ' + u.username} onClick={() => setModal(u)}><Pencil size={12} /></button>
          <button className="btn-ghost btn-sm" aria-label={'Deactivate ' + u.username} onClick={() => deactivate(u)}><Trash2 size={12} /></button></div> }]} />}
      {modal && <UserForm user={modal.user_id ? modal : null} pharmacies={(pharmacies.data && pharmacies.data.items) || []} onClose={() => setModal(null)} onDone={() => { setModal(null); users.reload(); }} />}
    </div>
  );
}
