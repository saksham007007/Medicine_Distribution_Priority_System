// SAKSHAM - application shell: routing, role-based navigation, layout.
import React, { useEffect, useState } from 'react';
import { HashRouter, Routes, Route, Navigate, NavLink, Link, useLocation } from 'react-router-dom';
import { LayoutDashboard, Users, ListOrdered, Pill, Boxes, Building2, Repeat, Truck, Clock, Receipt, Bell, Activity, UserCog, Settings as SettingsIcon, LogOut, Menu, X, HeartPulse } from 'lucide-react';
import { api } from './api.js';
import { AuthProvider, useAuth } from './auth.jsx';
import { ToastProvider, useAsync, Spinner, ErrorBox } from './ui.jsx';
import Login from './pages/Login.jsx';
import Dashboard from './pages/Dashboard.jsx';
import Patients from './pages/Patients.jsx';
import Queue from './pages/Queue.jsx';
import Medicines from './pages/Medicines.jsx';
import Inventory from './pages/Inventory.jsx';
import Pharmacies from './pages/Pharmacies.jsx';
import Alternatives from './pages/Alternatives.jsx';
import Distribution from './pages/Distribution.jsx';
import WaitingList from './pages/WaitingList.jsx';
import Transactions from './pages/Transactions.jsx';
import Notifications from './pages/Notifications.jsx';
import Analytics from './pages/Analytics.jsx';
import UsersPage from './pages/Users.jsx';
import SettingsPage from './pages/Settings.jsx';

const ALL = ['Admin', 'Pharmacist', 'Patient'], STAFF = ['Admin', 'Pharmacist'];
export const PAGES = [
  { to: '/', label: 'Dashboard', icon: LayoutDashboard, roles: ALL, el: <Dashboard /> },
  { to: '/patients', label: 'Patients', patientLabel: 'My Profile', icon: Users, roles: ALL, el: <Patients /> },
  { to: '/queue', label: 'Priority Queue', patientLabel: 'My Requests', icon: ListOrdered, roles: ALL, el: <Queue /> },
  { to: '/medicines', label: 'Medicines', icon: Pill, roles: ALL, el: <Medicines /> },
  { to: '/inventory', label: 'Inventory', icon: Boxes, roles: STAFF, el: <Inventory /> },
  { to: '/pharmacies', label: 'Pharmacies', icon: Building2, roles: ALL, el: <Pharmacies /> },
  { to: '/alternatives', label: 'Alternatives', icon: Repeat, roles: ALL, el: <Alternatives /> },
  { to: '/distribution', label: 'Distribution', icon: Truck, roles: STAFF, el: <Distribution /> },
  { to: '/waiting', label: 'Waiting List', icon: Clock, roles: ALL, el: <WaitingList /> },
  { to: '/transactions', label: 'Transactions', icon: Receipt, roles: ALL, el: <Transactions /> },
  { to: '/notifications', label: 'Notifications', icon: Bell, roles: ALL, el: <Notifications /> },
  { to: '/analytics', label: 'Analytics', icon: Activity, roles: STAFF, el: <Analytics /> },
  { to: '/users', label: 'User Management', icon: UserCog, roles: ['Admin'], el: <UsersPage /> },
  { to: '/settings', label: 'Settings', icon: SettingsIcon, roles: ALL, el: <SettingsPage /> },
];

function Layout({ children }) {
  const { user, logout } = useAuth();
  const [open, setOpen] = useState(false);
  const loc = useLocation();
  useEffect(() => setOpen(false), [loc.pathname]);
  const notes = useAsync(() => api.get('/api/notifications', { limit: 1 }), [loc.pathname]);
  const info = useAsync(() => api.get('/api/public/info'), []);
  useEffect(() => { const t = setInterval(notes.reload, 30000); return () => clearInterval(t); }, [notes.reload]);
  const unread = notes.data ? notes.data.unread : 0;
  const items = PAGES.filter((p) => p.roles.includes(user.role));

  return (
    <div className="flex min-h-screen">
      <aside data-testid="sidebar" data-open={open} className={(open ? 'fixed inset-y-0 left-0 z-30 flex' : 'hidden') + ' w-64 shrink-0 flex-col bg-brand-900 text-white md:sticky md:top-0 md:flex md:h-screen'}>
        <div className="flex items-center gap-2 px-4 py-4"><HeartPulse className="text-teal-500" /><div><div className="text-sm font-bold leading-tight">Medicine Distribution</div><div className="text-xs text-blue-200">Priority System · Bug Busters</div></div></div>
        <nav className="flex-1 space-y-1 overflow-y-auto px-2 pb-4" aria-label="Main">
          {items.map((p) => (
            <NavLink key={p.to} to={p.to} end={p.to === '/'} data-testid={'nav-' + p.to.replace('/', '') || 'nav-home'}
              className={({ isActive }) => 'flex items-center gap-3 rounded-lg px-3 py-2 text-sm ' + (isActive ? 'bg-teal-600 text-white' : 'text-blue-100 hover:bg-brand-700')}>
              <p.icon size={18} />{user.role === 'Patient' && p.patientLabel ? p.patientLabel : p.label}
            </NavLink>
          ))}
        </nav>
      </aside>
      <div className="flex min-w-0 flex-1 flex-col">
        <header className="sticky top-0 z-20 flex items-center justify-between border-b border-slate-200 bg-white px-4 py-2">
          <button className="btn-ghost md:hidden" aria-label="Toggle menu" aria-expanded={open} data-testid="menu-toggle" onClick={() => setOpen(!open)}>{open ? <X size={18} /> : <Menu size={18} />}</button>
          <div className="hidden text-sm text-slate-500 md:block">Welcome, <span className="font-semibold text-slate-800">{user.full_name}</span></div>
          <div className="flex items-center gap-3">
            <Link to="/notifications" aria-label="Notifications" className="relative rounded-lg p-2 hover:bg-slate-100"><Bell size={18} />
              {unread > 0 && <span data-testid="unread-badge" className="absolute -right-0.5 -top-0.5 rounded-full bg-red-600 px-1.5 text-[10px] font-bold text-white">{unread}</span>}</Link>
            <span className="rounded-full bg-teal-100 px-2 py-0.5 text-xs font-semibold text-teal-700" data-testid="role-badge">{user.role}</span>
            <button className="btn-ghost" onClick={() => logout()} aria-label="Log out"><LogOut size={16} /> <span className="hidden sm:inline">Log out</span></button>
          </div>
        </header>
        {info.data && <div className="border-b border-amber-200 bg-amber-50 px-4 py-1.5 text-xs text-amber-800" data-testid="demo-banner">{info.data.data_notice}</div>}
        <main className="flex-1 p-4 md:p-6">{children}</main>
      </div>
    </div>
  );
}

function Protected({ roles, children }) {
  const { user, loading } = useAuth();
  if (loading) return <Spinner label="Restoring session…" />;
  if (!user) return <Navigate to="/login" replace />;
  if (roles && !roles.includes(user.role)) return <Layout><ErrorBox error="You do not have access to this page." /></Layout>;
  return <Layout>{children}</Layout>;
}

function LoginRoute() {
  const { user, loading } = useAuth();
  if (loading) return <Spinner />;
  return user ? <Navigate to="/" replace /> : <Login />;
}

export default function App() {
  return (
    <HashRouter>
      <AuthProvider>
        <ToastProvider>
          <Routes>
            <Route path="/login" element={<LoginRoute />} />
            {PAGES.map((p) => <Route key={p.to} path={p.to} element={<Protected roles={p.roles}>{p.el}</Protected>} />)}
            <Route path="*" element={<Navigate to="/" replace />} />
          </Routes>
        </ToastProvider>
      </AuthProvider>
    </HashRouter>
  );
}
