// ANKIT - notifications for all roles; staff can message a patient.
import React, { useState } from 'react';
import { Bell, Send, CheckCheck } from 'lucide-react';
import { api } from '../api.js';
import { useAuth } from '../auth.jsx';
import { useAsync, PageHeader, Badge, Modal, Input, Textarea, ErrorBox, Spinner, SearchSelect, useToast, fmtDate } from '../ui.jsx';

function MessageForm({ onClose }) {
  const toast = useToast(); const [patient, setPatient] = useState(null), [title, setTitle] = useState(''), [message, setMessage] = useState(''), [error, setError] = useState('');
  async function submit(e) { e.preventDefault(); setError(''); try { await api.post('/api/notifications', { patient_id: patient.patient_id, title, message }); toast.success('Notification sent'); onClose(); } catch (err) { setError(err.message); } }
  return (
    <Modal title="Send notification to a patient" onClose={onClose}>
      <form onSubmit={submit} className="space-y-3">
        <SearchSelect label="Patient" picked={patient} onPick={setPatient} fetcher={(q) => api.get('/api/patients', { q, limit: 10 }).then((r) => r.items)} renderItem={(p) => `#${p.patient_id} ${p.name}`} />
        <Input label="Title" value={title} onChange={(e) => setTitle(e.target.value)} required minLength={3} maxLength={100} /><Textarea label="Message" value={message} onChange={(e) => setMessage(e.target.value)} required minLength={3} maxLength={500} />
        <ErrorBox error={error} /><div className="flex justify-end gap-2"><button type="button" className="btn-ghost" onClick={onClose}>Cancel</button><button className="btn-primary" disabled={!patient}><Send size={14} /> Send</button></div>
      </form>
    </Modal>
  );
}

export default function Notifications() {
  const { user } = useAuth(); const toast = useToast();
  const [compose, setCompose] = useState(false);
  const list = useAsync(() => api.get('/api/notifications', { limit: 100 }), []);
  async function read(n) { try { await api.post(`/api/notifications/${n.notification_id}/read`); list.reload(); } catch (e) { toast.error(e.message); } }
  async function readAll() { try { await api.post('/api/notifications/read-all'); toast.success('All marked as read'); list.reload(); } catch (e) { toast.error(e.message); } }
  return (
    <div>
      <PageHeader title="Notifications" subtitle={list.data ? `${list.data.unread} unread` : ''}>
        <button className="btn-ghost" onClick={readAll}><CheckCheck size={14} /> Mark all read</button>
        {user.role !== 'Patient' && <button className="btn-primary" onClick={() => setCompose(true)}><Send size={14} /> Message a patient</button>}
      </PageHeader>
      {list.loading && !list.data ? <Spinner /> : <ErrorBox error={list.error} onRetry={list.reload} />}
      {list.data && list.data.items.length === 0 && <div className="rounded-lg border border-dashed border-slate-300 p-6 text-center text-sm text-slate-500">No notifications.</div>}
      <ul className="space-y-2" data-testid="notification-list">
        {list.data && list.data.items.map((n) => (
          <li key={n.notification_id} className={'card flex items-start gap-3 ' + (n.is_read ? 'opacity-70' : 'border-l-4 border-l-brand-500')} data-read={n.is_read}>
            <Bell size={16} className="mt-1 text-brand-500" />
            <div className="flex-1"><div className="flex flex-wrap items-center gap-2"><span className="font-semibold">{n.title}</span><Badge value="None">{n.type}</Badge><span className="text-xs text-slate-400">{fmtDate(n.created_at)}</span></div><p className="text-sm text-slate-600">{n.message}</p></div>
            {!n.is_read && <button className="btn-ghost btn-sm" onClick={() => read(n)}>Mark read</button>}
          </li>))}
      </ul>
      {compose && <MessageForm onClose={() => { setCompose(false); list.reload(); }} />}
    </div>
  );
}
