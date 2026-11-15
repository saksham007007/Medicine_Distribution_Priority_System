import React from 'react';
const getPath = () => ((window.location.hash || '').replace(/^#/, '') || '/').split('?')[0];
const Ctx = React.createContext('/');
export function HashRouter({ children }) {
  const [path, setPath] = React.useState(getPath());
  React.useEffect(() => { const f = () => setPath(getPath()); window.addEventListener('hashchange', f); f(); return () => window.removeEventListener('hashchange', f); }, []);
  return <Ctx.Provider value={path}>{children}</Ctx.Provider>;
}
export function Route() { return null; }
export function Routes({ children }) {
  const path = React.useContext(Ctx);
  const routes = React.Children.toArray(children);
  const hit = routes.find((r) => r.props.path === path) || routes.find((r) => r.props.path === '*');
  return hit ? hit.props.element : null;
}
export function Navigate({ to }) {
  React.useEffect(() => { window.history.replaceState(null, '', '#' + to); window.dispatchEvent(new HashChangeEvent('hashchange')); }, [to]);
  return null;
}
export const useLocation = () => ({ pathname: React.useContext(Ctx) });
export const useNavigate = () => (to) => { window.location.hash = '#' + to; };
export function Link({ to, children, ...rest }) { return <a href={'#' + to} {...rest}>{children}</a>; }
export function NavLink({ to, end, className, children, ...rest }) {
  const path = React.useContext(Ctx);
  const active = end ? path === to : path === to || path.startsWith(to + '/');
  return <a href={'#' + to} className={typeof className === 'function' ? className({ isActive: active }) : className} aria-current={active ? 'page' : undefined} {...rest}>{children}</a>;
}
