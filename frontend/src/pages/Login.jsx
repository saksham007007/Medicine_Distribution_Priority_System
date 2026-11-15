// SAKSHAM - login and patient self-registration.
import React, { useState } from 'react';
import { HeartPulse } from 'lucide-react';
import { useAuth } from '../auth.jsx';
import { api } from '../api.js';
import { Input, ErrorBox, Tabs } from '../ui.jsx';

const DEMO = [['Admin', 'admin', 'Admin@123'], ['Pharmacist', 'pharmacist', 'Pharma@123'], ['Patient', 'patient', 'Patient@123']];

export default function Login() {
  const { login } = useAuth();
  const [tab, setTab] = useState('login');
  const [f, setF] = useState({ username: '', password: '', full_name: '', age: '', city: '', contact: '' });
  const [busy, setBusy] = useState(false), [error, setError] = useState(''), [ok, setOk] = useState('');
  const set = (k) => (e) => setF({ ...f, [k]: e.target.value });

  async function submit(e) {
    e.preventDefault(); setBusy(true); setError(''); setOk('');
    try {
      if (tab === 'login') await login(f.username.trim(), f.password);
      else {
        const body = { username: f.username.trim(), password: f.password, full_name: f.full_name };
        if (f.age) body.age = Number(f.age); if (f.city) body.city = f.city; if (f.contact) body.contact = f.contact;
        await api.post('/api/auth/register', body);
        setOk('Registration successful. You can now log in.'); setTab('login'); setF({ ...f, password: '' });
      }
    } catch (err) { setError(err.message); } finally { setBusy(false); }
  }

  return (
    <div className="flex min-h-screen items-center justify-center bg-gradient-to-br from-brand-900 via-brand-700 to-teal-700 p-4">
      <div className="w-full max-w-md rounded-2xl bg-white p-6 shadow-2xl">
        <div className="mb-4 flex items-center gap-3"><HeartPulse size={32} className="text-teal-600" />
          <div><h1 className="text-lg font-bold text-slate-900">Medicine Distribution Priority System</h1><p className="text-xs text-slate-500">Team Bug Busters · Graphic Era</p></div></div>
        <Tabs value={tab} onChange={(t) => { setTab(t); setError(''); }} tabs={[{ value: 'login', label: 'Log in' }, { value: 'register', label: 'Patient registration' }]} />
        <form onSubmit={submit} className="space-y-3">
          {tab === 'register' && <Input label="Full name" value={f.full_name} onChange={set('full_name')} required minLength={2} />}
          <Input label="Username" value={f.username} onChange={set('username')} required autoComplete="username" />
          <Input label="Password" type="password" value={f.password} onChange={set('password')} required autoComplete={tab === 'login' ? 'current-password' : 'new-password'} hint={tab === 'register' ? 'At least 8 characters with a letter and a digit' : undefined} />
          {tab === 'register' && (
            <div className="grid grid-cols-3 gap-2">
              <Input label="Age" type="number" min="0" max="120" value={f.age} onChange={set('age')} />
              <Input label="City" value={f.city} onChange={set('city')} />
              <Input label="Contact" value={f.contact} onChange={set('contact')} placeholder="+91-…" />
            </div>)}
          <ErrorBox error={error} />
          {ok && <div className="rounded-lg bg-teal-50 p-3 text-sm text-teal-700" role="status">{ok}</div>}
          <button className="btn-primary w-full" disabled={busy}>{busy ? 'Please wait…' : tab === 'login' ? 'Log in' : 'Create patient account'}</button>
        </form>
        <div className="mt-4 rounded-lg bg-slate-50 p-3 text-xs text-slate-600">
          <div className="mb-1 font-semibold">Demo accounts (sample data)</div>
          {DEMO.map(([r, u, p]) => <button key={u} type="button" className="mr-2 mt-1 rounded border border-slate-300 bg-white px-2 py-1 hover:bg-brand-50" onClick={() => { setTab('login'); setF({ ...f, username: u, password: p }); }}>{r}: {u}</button>)}
        </div>
      </div>
    </div>
  );
}
