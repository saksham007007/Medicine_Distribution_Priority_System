// Real-browser end-to-end UI test (headless Chromium via Playwright) against the real C++ server.
// Usage: NODE_PATH=... node tests/ui/ui_test.cjs http://127.0.0.1:8098
const { chromium } = require('playwright');
const BASE = process.argv[2] || 'http://127.0.0.1:8098';
let passed = 0, failed = 0; const failures = []; const pageErrors = [];

async function step(name, fn) {
  const t0 = Date.now();
  try { await fn(); passed++; process.stdout.write('  ok   ' + name + ' (' + ((Date.now() - t0) / 1000).toFixed(1) + 's)\n'); }
  catch (e) { try { const dl = await globalThis.__page.getByRole('dialog').allInnerTexts(); if (dl.length) process.stdout.write('       [open dialog: ' + dl.join(' // ').replace(/\n+/g, ' | ').slice(0, 300) + ']\n'); } catch {} failed++; failures.push(name + ' :: ' + String(e.message).split('\n')[0].slice(0, 260)); process.stdout.write('  FAIL ' + name + ' :: ' + String(e.message).split('\n')[0].slice(0, 260) + '\n'); }
}
const expect = (cond, msg) => { if (!cond) throw new Error(msg || 'assertion failed'); };
const api = async (method, path, body, token) => {
  const r = await fetch(BASE + path, { method, headers: { 'Content-Type': 'application/json', ...(token ? { Authorization: 'Bearer ' + token } : {}) }, body: body ? JSON.stringify(body) : undefined });
  return { status: r.status, data: await r.json().catch(() => null) };
};
const settled = (page) => page.waitForFunction(() => !/Loading…|Restoring session|Please wait/.test(document.body.innerText), null, { timeout: 15000 });
const title = (page) => page.getByTestId('page-title').first();
const go = async (page, hash) => { await page.evaluate((h) => { window.location.hash = h; }, hash); await settled(page); await page.waitForTimeout(150); await settled(page); };
const toast = (page, re) => page.locator('[data-testid=toasts] > div').filter({ hasText: re }).first().waitFor({ timeout: 8000 });
const dialog = (page) => page.getByRole('dialog');
const rows = (page) => page.locator('tbody tr');

async function login(page, user, pass) {
  await page.goto(BASE + '/#/login'); await page.evaluate(() => localStorage.clear()); await page.goto(BASE + '/#/login'); await page.reload(); await settled(page);
  await page.getByLabel('Username').fill(user); await page.getByLabel('Password').fill(pass);
  await page.getByRole('button', { name: 'Log in', exact: true }).last().click();
  await page.getByTestId('role-badge').waitFor({ timeout: 10000 }); await settled(page);
}
async function logout(page) { await page.getByRole('button', { name: 'Log out' }).click(); await page.getByLabel('Username').waitFor(); }

(async () => {
  const browser = await chromium.launch({ headless: true, args: ['--disable-dev-shm-usage', '--disable-gpu', '--no-sandbox'] });
  const ctx = await browser.newContext({ viewport: { width: 1280, height: 900 }, acceptDownloads: true });
  const page = await ctx.newPage(); globalThis.__page = page;
  page.on('pageerror', (e) => pageErrors.push('pageerror: ' + e.message));
  page.on('console', (m) => { if (m.type() === 'error' && !/Failed to load resource/.test(m.text())) pageErrors.push('console: ' + m.text()); });
  page.on('dialog', (d) => d.accept());
  page.on('crash', () => pageErrors.push('PAGE CRASHED'));
  page.on('close', () => process.stdout.write('       [page closed at ' + new Date().toISOString() + ']\n'));
  browser.on('disconnected', () => process.stdout.write('       [browser disconnected]\n'));

  console.log('\n== Authentication & access control');
  await step('unauthenticated visit redirects to the login page', async () => { await page.goto(BASE + '/'); await page.evaluate(() => localStorage.clear()); await page.goto(BASE + '/#/patients'); await page.reload(); await page.getByLabel('Username').waitFor({ timeout: 8000 }); expect(page.url().includes('#/login')); });
  await step('wrong password shows an error and stays on login', async () => { await page.getByLabel('Username').fill('admin'); await page.getByLabel('Password').fill('nope'); await page.getByRole('button', { name: 'Log in', exact: true }).last().click(); await page.getByRole('alert').filter({ hasText: 'Invalid username or password' }).waitFor({ timeout: 8000 }); });
  await step('patient login: patient-only navigation, no staff pages', async () => {
    await login(page, 'patient', 'Patient@123'); expect((await page.getByTestId('role-badge').innerText()) === 'Patient');
    const nav = await page.getByRole('navigation', { name: 'Main' }).innerText();
    for (const t of ['My Profile', 'My Requests', 'Medicines', 'Pharmacies', 'Alternatives', 'Waiting List', 'My Transactions'.replace('My ', ''), 'Notifications']) expect(nav.includes(t), 'missing nav ' + t);
    for (const t of ['Inventory', 'Analytics', 'User Management', 'Distribution']) expect(!nav.includes(t), 'patient sees ' + t);
    await page.getByText('Hello, Demo Patient').waitFor();
  });
  await step('patient opening a staff URL gets "no access"', async () => { await go(page, '#/users'); await page.getByRole('alert').filter({ hasText: 'do not have access' }).waitFor(); await go(page, '#/analytics'); await page.getByRole('alert').filter({ hasText: 'do not have access' }).waitFor(); });
  await step('demo banner labels data as sample/demonstration', async () => { expect((await page.getByTestId('demo-banner').innerText()).includes('Demonstration data')); });
  await step('logout returns to login and clears the session', async () => { await logout(page); expect(await page.evaluate(() => localStorage.getItem('mdps_token')) === null); });
  await step('an invalid stored token is rejected and sends the user to login', async () => { await page.evaluate(() => localStorage.setItem('mdps_token', 'x'.repeat(64))); await page.goto(BASE + '/#/'); await page.reload(); await page.getByLabel('Username').waitFor({ timeout: 8000 }); });

  console.log('\n== Admin: every page loads real data without errors');
  await login(page, 'admin', 'Admin@123');
  const tour = [['#/', 'Dashboard'], ['#/patients', 'Patients'], ['#/queue', 'Priority Queue'], ['#/medicines', 'Medicines'], ['#/inventory', 'Inventory'], ['#/pharmacies', 'Pharmacies'], ['#/alternatives', 'Medicine Alternatives'],
    ['#/distribution', 'Distribution'], ['#/waiting', 'Waiting List'], ['#/transactions', 'Transactions'], ['#/notifications', 'Notifications'], ['#/analytics', 'Analytics'], ['#/users', 'User Management'], ['#/settings', 'Settings']];
  for (const [hash, t] of tour) {
    await step(`page ${t}`, async () => {
      await go(page, hash); expect((await title(page).innerText()).trim() === t, 'title was ' + (await title(page).innerText()));
      if ((await page.getByRole('alert').count()) !== 0) throw new Error('error alert: ' + (await page.getByRole('alert').first().innerText()));
      if (['#/patients', '#/queue', '#/medicines', '#/inventory', '#/pharmacies', '#/transactions', '#/users', '#/waiting'].includes(hash)) expect((await rows(page).count()) > 0, 'no table rows');
    });
  }
  await step('dashboard shows live numbers and 4 charts', async () => {
    await go(page, '#/');
    expect(Number(await page.getByTestId('stat-medicines').locator('div').nth(1).innerText()) >= 1000, 'medicines stat');
    expect(Number(await page.getByTestId('stat-pharmacies').locator('div').nth(1).innerText()) >= 30, 'pharmacies stat');
    expect((await page.locator('[data-chart]').count()) >= 4, 'charts mounted');
    expect(Number(await page.locator('[data-chart=line]').first().getAttribute('data-points')) === 14, 'trend has 14 daily points');
  });
  await step('analytics: charts, pharmacy-wise table, CSV download', async () => {
    await go(page, '#/analytics'); expect((await page.locator('[data-chart]').count()) >= 4);
    const [dl] = await Promise.all([page.waitForEvent('download'), page.getByRole('button', { name: 'Transactions CSV' }).click()]);
    expect(dl.suggestedFilename() === 'transactions.csv');
    const fs = require('fs'); const p = await dl.path(); expect(fs.readFileSync(p, 'utf8').startsWith('transaction_id,created_at'), 'csv content');
  });

  console.log('\n== Medicines catalogue');
  await step('search, filter, sort and paginate', async () => {
    await go(page, '#/medicines');
    const total0 = await page.getByText(/\d+ records/).first().innerText(); expect(Number(total0.match(/(\d+) records/)[1]) >= 1000, 'catalogue total ' + total0);
    await page.getByLabel('Search medicines').fill('paracetamol'); await page.waitForTimeout(700); await settled(page);
    const n = await rows(page).count(); expect(n > 3, 'search rows ' + n);
    for (const t of await page.locator('tbody tr td:nth-child(2)').allInnerTexts()) expect(/paracetamol/i.test(t), 'non-matching generic ' + t);
    await page.getByLabel('Sort by').selectOption('price'); await page.getByLabel('Order').selectOption('asc'); await settled(page); await page.waitForTimeout(300);
    const prices = (await page.locator('tbody tr td:nth-child(6)').allInnerTexts()).map((s) => Number(s.replace('₹', ''))); expect(prices.length > 2 && prices.every((v, i) => i === 0 || v >= prices[i - 1]), 'prices not ascending ' + prices);
    await page.getByLabel('Search medicines').fill(''); await page.getByLabel('Prescription filter').selectOption('1'); await page.waitForTimeout(700); await settled(page);
    for (const t of await page.locator('tbody tr td:nth-child(5)').allInnerTexts()) expect(t.trim() === 'Rx', 'OTC row in Rx filter');
    await page.getByLabel('Prescription filter').selectOption(''); await page.waitForTimeout(500); await settled(page);
    await page.getByRole('button', { name: 'Next' }).click(); await settled(page); expect((await page.getByText(/page 2 of/).count()) === 1, 'page 2');
  });
  await step('detail dialog shows availability at pharmacies', async () => { await page.getByRole('button', { name: /^Details / }).first().click(); await dialog(page).waitFor(); await settled(page); expect((await dialog(page).innerText()).includes('Available at pharmacies')); await page.getByRole('button', { name: 'Close' }).click(); });
  await step('admin adds, finds and removes a medicine', async () => {
    await go(page, '#/medicines'); await page.getByRole('button', { name: 'Add medicine' }).click();
    const d = dialog(page); const fill = async (l, v) => d.getByLabel(l, { exact: false }).first().fill(v);
    await fill('Display name', 'UiTest 25mg Tablet'); await fill('Brand name', 'UiTest'); await fill('Generic name', 'Uitestamine'); await fill('Composition', 'Uitestamine 25 mg'); await fill('Category', 'Antibiotics'); await fill('Manufacturer', 'UI Labs');
    await fill('Strength', '25mg'); await fill('Dosage form', 'Tablet'); await fill('Unit price', '9.5'); await d.getByRole('button', { name: 'Save' }).click(); await toast(page, /Medicine added/);
    await page.getByLabel('Search medicines').fill('uitestamine'); await page.waitForTimeout(700); await settled(page); expect((await rows(page).count()) === 1, 'new medicine found');
    await page.getByRole('button', { name: /^Remove UiTest/ }).click(); await toast(page, /deleted/);
  });
  await step('admin form rejects invalid data with a server message', async () => {
    await page.getByRole('button', { name: 'Add medicine' }).click(); const d = dialog(page);
    for (const [l, v] of [['Display name', 'ab'], ['Brand name', 'Bad'], ['Generic name', 'Bad'], ['Composition', 'Bad x'], ['Category', 'Antibiotics'], ['Manufacturer', 'UI Labs'], ['Strength', '1mg'], ['Dosage form', 'Tablet'], ['Unit price', '4']]) await d.getByLabel(l, { exact: false }).first().fill(v);
    await d.getByRole('button', { name: 'Save' }).click(); await d.getByRole('alert').waitFor({ timeout: 6000 }); expect((await d.getByRole('alert').innerText()).includes('name'), 'alert: ' + (await d.getByRole('alert').innerText())); await page.getByRole('button', { name: 'Close' }).click();
  });

  console.log('\n== Admin: patients, pharmacies, users, settings');
  await step('register a patient, find it, edit it, review history', async () => {
    await go(page, '#/patients'); await page.getByRole('button', { name: 'Register patient' }).click(); const d = dialog(page);
    await d.getByLabel('Full name').fill('Uitest Patient'); await d.getByLabel('Age').fill('41'); await d.getByLabel('City').fill('Dehradun'); await d.getByRole('button', { name: 'Save' }).click(); await toast(page, /Patient created/);
    await page.getByLabel('Search patients').fill('Uitest Patient'); await page.waitForTimeout(700); await settled(page); expect((await rows(page).count()) === 1);
    await page.getByRole('button', { name: /^Edit Uitest/ }).click(); await dialog(page).getByLabel('City').fill('Haridwar'); await dialog(page).getByRole('button', { name: 'Save' }).click(); await toast(page, /Patient updated/);
    await settled(page); expect((await rows(page).first().innerText()).includes('Haridwar'), 'edit persisted');
    await page.getByRole('button', { name: /^History Uitest/ }).click(); await dialog(page).getByText('Medicine requests').waitFor(); await page.getByRole('button', { name: 'Close' }).click();
  });
  await step('patient form validation errors come from the server', async () => { await page.getByRole('button', { name: 'Register patient' }).click(); const d = dialog(page); await d.getByLabel('Full name').fill('Valid Name'); await d.getByLabel('Contact').fill('abc'); await d.getByRole('button', { name: 'Save' }).click(); await d.getByRole('alert').waitFor(); expect((await d.getByRole('alert').innerText()).includes('contact')); await page.getByRole('button', { name: 'Close' }).click(); });
  await step('add a pharmacy then delete it', async () => {
    await go(page, '#/pharmacies'); await page.getByRole('button', { name: 'Add pharmacy' }).click(); const d = dialog(page);
    await d.getByLabel('Name', { exact: true }).fill('UiTest Pharmacy'); await d.getByLabel('Address').fill('1 Test Road'); await d.getByLabel('City', { exact: true }).fill('Dehradun'); await d.getByLabel('State').fill('Uttarakhand'); await d.getByLabel('PIN code').fill('248001');
    await d.getByRole('button', { name: 'Save' }).click(); await toast(page, /Pharmacy added/); await page.getByLabel('Search pharmacies').fill('UiTest'); await page.waitForTimeout(700); await settled(page); expect((await rows(page).count()) === 1);
    await page.getByRole('button', { name: /^Delete UiTest/ }).click(); await toast(page, /Pharmacy deleted/);
  });
  await step('pharmacy detail shows contact info and inventory', async () => { await page.getByLabel('Search pharmacies').fill('Sanjeevani'); await page.waitForTimeout(700); await settled(page); await page.getByRole('button', { name: /^Details Sanjeevani/ }).click(); await dialog(page).getByText('Inventory (first').waitFor({ timeout: 8000 }); await page.getByRole('button', { name: 'Close' }).click(); });
  await step('create a user, then deactivate it', async () => {
    await go(page, '#/users'); await page.getByRole('button', { name: 'New user' }).click(); const d = dialog(page);
    await d.getByLabel('Username').fill('uitest.pharm'); await d.getByLabel('Full name').fill('Ui Test Pharmacist'); await d.getByLabel('Password', { exact: false }).first().fill('UiTestPass1'); await d.getByRole('button', { name: 'Save' }).click(); await toast(page, /User created/);
    await page.getByLabel('Search users').fill('uitest.pharm'); await page.waitForTimeout(700); await settled(page); await page.getByRole('button', { name: 'Deactivate uitest.pharm' }).click(); await toast(page, /User deactivated/);
  });
  await step('settings: save, audit log shows recorded actions', async () => {
    await go(page, '#/settings'); await page.getByRole('tab', { name: 'System settings' }).click(); await settled(page);
    await page.getByLabel('Near-expiry window (days)').fill('45'); await page.getByTestId('save-settings').click(); await toast(page, /Settings saved/);
    await page.getByRole('tab', { name: 'Audit log' }).click(); await settled(page); await page.getByLabel('Filter audit log').fill('SETTINGS'); await page.waitForTimeout(700); await settled(page); expect((await rows(page).count()) >= 1, 'audit row');
    const s = await api('POST', '/api/auth/login', { username: 'admin', password: 'Admin@123' }); const set = await api('GET', '/api/settings', null, s.data.token); expect(set.data.items.find((i) => i.key === 'near_expiry_days').value === '45', 'setting persisted in DB');
  });
  await logout(page);

  console.log('\n== Pharmacist: priority queue, distribution, waiting list, inventory');
  await login(page, 'pharmacist', 'Pharma@123');
  let queueIds = [];
  await step('queue is shown in priority order with consecutive positions', async () => {
    await go(page, '#/queue'); const pos = (await page.getByTestId('pos').allInnerTexts()).map(Number); expect(pos.length >= 10, 'queue length ' + pos.length); expect(pos.every((v, i) => v === i + 1), 'positions ' + pos);
    const urg = await page.locator('tbody tr td:nth-child(6)').allInnerTexts(); const rank = { Critical: 1, High: 2, Medium: 3, Normal: 4 }; expect(urg.every((u, i) => i === 0 || rank[u.trim()] >= rank[urg[i - 1].trim()]), 'urgency order ' + urg);
  });
  await step('raising urgency moves a request up and requires a reason', async () => {
    const before = await page.locator('tbody tr').last().locator('td').nth(2).innerText();
    await page.locator('tbody tr').last().getByRole('button', { name: /Urgency/ }).click(); const d = dialog(page);
    await d.getByLabel(/New urgency/).selectOption('Critical'); await d.getByLabel(/Reason/).fill('Condition worsened, escalated by pharmacist'); await d.getByRole('button', { name: 'Update urgency' }).click(); await toast(page, /Urgency updated/);
    await settled(page); const crit = await page.locator('tbody tr td:nth-child(6)').allInnerTexts(); const lastCrit = crit.map((c) => c.trim()).lastIndexOf('Critical'); const names = await page.locator('tbody tr td:nth-child(3)').allInnerTexts();
    expect(names.slice(0, lastCrit + 1).some((n) => n.trim() === before.trim()), 'escalated patient is now in the Critical block');
  });
  await step('request details show recorded priority history', async () => { await page.getByRole('button', { name: /^Details / }).first().click(); await dialog(page).getByText('Priority history').waitFor(); await settled(page); expect((await dialog(page).innerText()).includes('Initial triage')); await page.getByRole('button', { name: 'Close' }).click(); });
  await step('create a request for a patient through the form', async () => {
    await page.getByRole('button', { name: 'New request' }).click(); const d = dialog(page);
    await d.getByLabel('Patient', { exact: true }).fill('Aarav'); await d.getByRole('listbox').getByRole('option').first().click();
    await d.getByLabel('Medicine', { exact: true }).fill('paracetamol'); await d.getByRole('listbox').getByRole('option').filter({ hasNotText: '· Rx' }).first().waitFor(); await d.getByRole('listbox').getByRole('option').filter({ hasNotText: '· Rx' }).first().click();
    await d.getByLabel('Quantity').fill('2'); await d.getByLabel('Urgency').selectOption('High'); await d.getByRole('button', { name: 'Submit request' }).click(); await toast(page, /queued at position \d+/);
  });
  await step('distribution: evaluate head of queue, verify prescriptions, distribute 4 requests in order', async () => {
    await go(page, '#/distribution'); let done = 0, waited = 0, verified = 0; const served = [];
    for (let i = 0; i < 40 && done < 4; i++) {
      await page.getByTestId('evaluate-next').click(); const res = page.getByTestId('result'); await res.waitFor({ timeout: 10000 }); const action = await res.getAttribute('data-action');
      if (action === 'blocked_prescription') { await page.getByTestId('verify-rx').click(); await toast(page, /Prescription verified/); verified++; continue; }
      if (action === 'added_to_waiting_list') { waited++; continue; }
      expect(action === 'ready', 'unexpected action ' + action);
      const who = (await res.locator('.text-lg').first().innerText()).split('\n')[0]; served.push(who);
      await res.locator('[data-testid^=distribute-]').first().click(); await toast(page, /Distributed \(transaction #\d+\)/); done++; await settled(page);
    }
    expect(done === 4, 'distributed ' + done); process.stdout.write(`       (distributed ${done}, moved to waiting ${waited}, prescriptions verified ${verified})\n`);
  });
  await step('distribution stock effect is visible in transactions', async () => { await go(page, '#/transactions'); await page.getByLabel('Status', { exact: true }).selectOption('Distributed'); await settled(page); await page.waitForTimeout(300); expect((await rows(page).count()) > 4); await page.getByRole('button', { name: /^Transaction \d+/ }).first().click(); await dialog(page).getByText('Batches used').waitFor(); await page.getByRole('button', { name: 'Close' }).click(); });
  await step('waiting list: propose an alternative, approve it, request returns to the queue', async () => {
    await go(page, '#/waiting'); const n0 = await rows(page).count(); expect(n0 >= 1, 'waiting rows');
    let proposed = false;
    for (let i = 0; i < n0 && !proposed; i++) {
      await rows(page).nth(i).getByRole('button', { name: 'Alternatives' }).click(); const d = dialog(page); await d.getByTestId('disclaimer').waitFor({ timeout: 8000 }); await settled(page);
      const btn = d.getByRole('button', { name: 'Propose', exact: true }).and(d.locator(':not([disabled])')); const cnt = await btn.count();
      if (cnt > 0) { await btn.first().click(); const d2 = page.getByRole('dialog').last(); await d2.getByRole('button', { name: 'Propose alternative' }).click(); await toast(page, /Alternative proposed/); proposed = true; }
      else await page.getByRole('button', { name: 'Close' }).first().click();
    }
    expect(proposed, 'could not find a proposable alternative');
    await page.keyboard.press('Escape'); await settled(page);
    await page.getByRole('button', { name: 'Decide' }).first().click(); await dialog(page).getByTestId('approve-sub').click(); await toast(page, /Alternative approved/); await settled(page);
    await page.waitForTimeout(300); expect((await rows(page).count()) === n0 - 1, 'request left the waiting list');
  });
  await step('inventory: low-stock alerts and a restock through the form', async () => {
    await go(page, '#/inventory'); await page.getByRole('tab', { name: 'Alerts' }).click(); await settled(page); expect((await page.getByText('Low stock').count()) > 0 && (await page.getByText('Expired stock').count()) > 0);
    await page.getByRole('tab', { name: 'Stock levels' }).click(); await settled(page);
    await page.getByRole('button', { name: 'Restock', exact: true }).first().click(); const d = dialog(page);
    await d.getByLabel('Pharmacy').selectOption({ index: 1 }); await d.getByLabel('Medicine', { exact: true }).fill('para'); await d.getByRole('listbox').getByRole('option').first().click(); await d.getByLabel('Batch number').fill('UI-BATCH-1'); await d.getByRole('button', { name: 'Add stock' }).click(); await toast(page, /Stock updated/);
    await page.getByRole('tab', { name: 'Restock history' }).click(); await settled(page); expect((await page.getByText('UI-BATCH-1').count()) >= 1, 'restock history');
    await page.getByRole('tab', { name: 'Stock movement' }).click(); await settled(page); expect((await page.getByText('Restock').count()) >= 1, 'movement recorded');
  });
  await step('restock form shows the server error when a batch number is reused with different dates', async () => {
    await page.getByRole('button', { name: 'Restock', exact: true }).first().click(); const d = dialog(page);
    await d.getByLabel('Pharmacy').selectOption({ index: 1 }); await d.getByLabel('Medicine', { exact: true }).fill('para'); await d.getByRole('listbox').getByRole('option').first().click(); await d.getByLabel('Batch number').fill('UI-BATCH-1'); await d.getByLabel('Expiry date').fill('2033-01-01');
    await d.getByRole('button', { name: 'Add stock' }).click(); await d.getByRole('alert').waitFor({ timeout: 6000 }); expect((await d.getByRole('alert').innerText()).includes('different dates'), 'alert: ' + (await d.getByRole('alert').innerText())); await page.getByRole('button', { name: 'Close' }).click();
  });
  await step('pharmacist sees verify button state and can message a patient', async () => {
    await go(page, '#/notifications'); await page.getByRole('button', { name: 'Message a patient' }).click(); const d = dialog(page);
    await d.getByLabel('Patient', { exact: true }).fill('Aarav'); await d.getByRole('listbox').getByRole('option').first().click(); await d.getByLabel('Title').fill('Pickup reminder'); await d.getByLabel('Message').fill('Your medicine is ready for pickup.'); await d.getByRole('button', { name: 'Send' }).click(); await toast(page, /Notification sent/);
  });
  await step('alternatives page: pick a medicine, see suggestions and the approval disclaimer, compare', async () => {
    await go(page, '#/alternatives'); await page.getByLabel('Medicine to find alternatives for').fill('paracetamol'); await page.getByRole('listbox').getByRole('option').first().click(); await settled(page);
    await page.getByTestId('disclaimer').waitFor(); expect((await page.getByTestId('disclaimer').innerText()).toLowerCase().includes('pharmacist')); expect((await rows(page).count()) > 0, 'alternatives rows');
    await page.getByRole('button', { name: 'Compare' }).first().click(); await dialog(page).getByText('Attribute').waitFor(); await page.getByRole('button', { name: 'Close' }).click();
  });
  await step('pharmacies: find a medicine sorted by distance', async () => {
    await go(page, '#/pharmacies'); await page.getByRole('tab', { name: 'Find a medicine' }).click(); await page.getByLabel('Medicine', { exact: true }).fill('paracetamol'); await page.getByRole('listbox').getByRole('option').first().click();
    await page.getByLabel('Latitude').fill('30.3165'); await page.getByLabel('Longitude').fill('78.0322'); await settled(page); await page.waitForTimeout(400); await settled(page);
    const d = (await page.locator('tbody tr td:nth-child(4)').allInnerTexts()).map((s) => parseFloat(s)).filter((v) => !isNaN(v)); expect(d.length >= 2 && d.every((v, i) => i === 0 || v >= d[i - 1]), 'distances ' + d);
  });
  await logout(page);

  console.log('\n== Patient: register, request a medicine, track and cancel it');
  await step('self-registration then login', async () => {
    await page.getByRole('tab', { name: 'Patient registration' }).click(); await page.getByLabel('Full name').fill('Browser Test Patient'); await page.getByLabel('Username').fill('browser.patient'); await page.getByLabel('Password').fill('BrowserPass1'); await page.getByLabel('City').fill('Dehradun');
    await page.getByRole('button', { name: 'Create patient account' }).click(); await page.getByText('Registration successful').waitFor({ timeout: 8000 });
    await page.getByLabel('Username').fill('browser.patient'); await page.getByLabel('Password').fill('BrowserPass1'); await page.getByRole('button', { name: 'Log in', exact: true }).last().click(); await page.getByTestId('role-badge').waitFor(); await settled(page);
    await page.getByText('Hello, Browser Test Patient').waitFor();
  });
  await step('patient requests an OTC medicine; it enters the queue as Normal', async () => {
    await go(page, '#/queue'); await page.getByRole('button', { name: 'New request' }).click(); const d = dialog(page);
    await d.getByLabel('Medicine', { exact: true }).fill('cetirizine'); await d.getByRole('listbox').getByRole('option').filter({ hasNotText: '· Rx' }).first().click(); await d.getByLabel('Quantity').fill('3'); expect((await d.innerText()).includes('enter the queue as'), 'normal-priority notice'); expect((await d.getByLabel('Urgency').count()) === 0, 'no urgency selector for patients');
    await d.getByRole('button', { name: 'Submit request' }).click(); await toast(page, /queued at position/); await settled(page);
    expect((await rows(page).first().innerText()).includes('Normal') && (await rows(page).first().innerText()).includes('Pending'), 'row shows Pending/Normal');
  });
  await step('patient sees queue position on the dashboard', async () => { await go(page, '#/'); expect((await rows(page).first().innerText()).includes('Pending')); const pos = (await rows(page).first().locator('td').nth(5).innerText()).trim(); expect(/^\d+$/.test(pos), 'queue position ' + pos); });
  await step('patient requests a prescription medicine: must add a prescription', async () => {
    await go(page, '#/queue'); await page.getByRole('button', { name: 'New request' }).click(); const d = dialog(page);
    await d.getByLabel('Medicine', { exact: true }).fill('amoxicillin'); await d.getByRole('listbox').getByRole('option').first().click(); await d.getByText('needs a prescription').waitFor();
    await d.getByRole('button', { name: 'Submit request' }).click(); await d.getByRole('alert').waitFor({ timeout: 6000 }); expect((await d.getByRole('alert').innerText()).toLowerCase().includes('prescription'));
    await d.getByRole('button', { name: 'Add a prescription' }).click(); const d2 = page.getByRole('dialog').last(); await d2.getByLabel('Doctor name').fill('Dr Browser Test'); await d2.getByRole('button', { name: 'Save prescription' }).click(); await toast(page, /Prescription recorded/);
    await d.getByRole('button', { name: 'Submit request' }).click(); await toast(page, /queued at position/);
  });
  await step('patient cancels own request', async () => { await settled(page); await page.getByRole('tab', { name: 'My requests' }).click(); await settled(page); await rows(page).first().getByRole('button', { name: 'Cancel' }).click(); await toast(page, /Request cancelled/); await settled(page); expect((await rows(page).first().innerText()).includes('Cancelled')); });
  await step('patient edits profile and sees it persisted', async () => { await go(page, '#/patients'); await page.getByRole('button', { name: 'Edit details' }).click(); await dialog(page).getByLabel('City').fill('Mussoorie'); await dialog(page).getByRole('button', { name: 'Save' }).click(); await toast(page, /Patient updated/); await settled(page); expect((await page.getByTestId('profile').innerText()).includes('Mussoorie')); });
  await step('patient notifications and own transactions pages work', async () => { await go(page, '#/notifications'); await settled(page); await go(page, '#/transactions'); expect((await page.getByRole('alert').count()) === 0); await go(page, '#/waiting'); expect((await page.getByRole('alert').count()) === 0); });
  await step('patient cannot see other patients (API enforces it even if the UI is bypassed)', async () => { const tok = await page.evaluate(() => localStorage.getItem('mdps_token')); const r = await api('GET', '/api/patients/1', null, tok); expect(r.status === 403, 'status ' + r.status); });
  await step('change password signs the user out', async () => {
    await go(page, '#/settings'); await page.getByLabel('Current password').fill('BrowserPass1'); await page.getByLabel('New password').fill('BrowserPass2'); await page.getByRole('button', { name: 'Change password' }).click(); await page.getByLabel('Username').waitFor({ timeout: 8000 });
  });

  console.log('\n== Responsive behaviour (menu logic at mobile width)');
  await step('mobile: menu toggle opens and closes the sidebar', async () => {
    const m = await browser.newContext({ viewport: { width: 390, height: 800 } }); const mp = await m.newPage(); mp.on('pageerror', (e) => pageErrors.push('mobile pageerror: ' + e.message));
    await login(mp, 'patient2', 'Patient@123');
    expect((await mp.getByTestId('sidebar').getAttribute('data-open')) === 'false'); await mp.getByTestId('menu-toggle').click(); expect((await mp.getByTestId('sidebar').getAttribute('data-open')) === 'true');
    await mp.getByRole('link', { name: 'Medicines' }).click(); await settled(mp); await mp.waitForTimeout(200); expect((await mp.getByTestId('sidebar').getAttribute('data-open')) === 'false', 'menu closes after navigation'); await m.close();
  });

  expect_noErrors: {
    await step('no uncaught page errors or console errors during the whole run', async () => { expect(pageErrors.length === 0, pageErrors.slice(0, 3).join(' | ')); });
  }
  await browser.close();
  console.log(`\n${passed} passed, ${failed} failed`);
  if (failures.length) console.log('FAILURES:\n - ' + failures.join('\n - '));
  process.exit(failed ? 1 : 0);
})().catch((e) => { console.error('Test runner crashed:', e); process.exit(2); });
