'use strict';

/* ---------- Constantes ---------- */

const KEY = 'lifereset.v1';

const PROMPTS = [
  'O que eu estou evitando que, se eu fizesse, mudaria tudo?',
  'Se eu continuar exatamente como hoje por 5 anos, onde vou estar?',
  'Que pequena vitória de hoje prova que estou virando quem quero ser?',
  'O que eu faria hoje se não tivesse medo de parecer ridículo?',
  'Qual hábito está roubando mais energia de mim?',
  'Que verdade sobre a minha vida eu ainda não admiti?',
  'Quem eu preciso ser para alcançar a minha meta de 1 ano?',
  'O que posso eliminar hoje para ter mais foco?',
  'Pelo que eu sou grato hoje?',
  'Qual foi o momento em que me senti mais vivo hoje?',
  'Se alguém assistisse ao meu dia, diria que eu quero o quê da vida?',
  'O que eu aprendi hoje que quero lembrar?',
  'Onde eu fui reativo hoje, em vez de agir com intenção?',
  'Qual é a menor ação que eu posso fazer amanhã para avançar?'
];

const VISION_FIELDS = [
  ['antiVision', 'Antivisão',
    'A vida que você NÃO quer daqui a 5–10 anos se nada mudar. Seja específico e honesto: como é um dia comum nessa vida? Como você se sente?', 6],
  ['vision', 'Visão',
    'A vida que você quer. Como é um dia comum: onde mora, com quem está, o que faz, como se sente?', 6],
  ['yearGoal', 'Meta de 1 ano',
    'Uma conquista concreta que, em 12 meses, prova que você está no caminho da visão.', 3],
  ['monthProject', 'Projeto do mês',
    'O projeto que move a meta de 1 ano agora. Específico e com uma entrega clara.', 3],
  ['constraints', 'Restrições',
    'Regras que você não negocia. Ex.: sem redes sociais antes do meio-dia; nada de tela depois das 23h.', 3]
];

const DEFAULT_LEVERS = [
  'Acordar no horário, sem celular na 1ª hora',
  '90 min de trabalho focado no projeto do mês',
  'Treino ou 30 min de movimento',
  'Ler 10 páginas',
  'Revisar o dia antes de dormir'
];

const DEFAULT_STEPS = [
  'Revisar a semana: o que fiz e o que ficou pendente',
  'Zerar as caixas de entrada (e-mail, mensagens, anotações)',
  'Conferir a agenda dos próximos 7 dias',
  'Atualizar o projeto do mês e a próxima entrega',
  'Definir as 3 prioridades da semana',
  'Organizar a casa e o espaço de trabalho',
  'Planejar refeições e compras',
  'Conferir as finanças da semana',
  'Reservar um tempo de descanso de verdade'
];

const TITLES = { hoje: 'Hoje', visao: 'Visão', semana: 'Reset semanal', progresso: 'Progresso' };

/* ---------- Utilidades ---------- */

const uid = () => Math.random().toString(36).slice(2, 10);
const pad = (n) => String(n).padStart(2, '0');
const dkey = (d) => `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}`;
const parseKey = (k) => { const [y, m, d] = k.split('-').map(Number); return new Date(y, m - 1, d); };
const addDays = (d, n) => { const x = new Date(d.getFullYear(), d.getMonth(), d.getDate()); x.setDate(x.getDate() + n); return x; };
const cap = (s) => s.charAt(0).toUpperCase() + s.slice(1);
const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));

function weekStart(d) {
  const x = new Date(d.getFullYear(), d.getMonth(), d.getDate());
  const day = x.getDay() || 7; // segunda = 1 ... domingo = 7
  x.setDate(x.getDate() - day + 1);
  return x;
}

function isoWeek(d) {
  const t = new Date(Date.UTC(d.getFullYear(), d.getMonth(), d.getDate()));
  const day = t.getUTCDay() || 7;
  t.setUTCDate(t.getUTCDate() + 4 - day);
  const y0 = new Date(Date.UTC(t.getUTCFullYear(), 0, 1));
  const w = Math.ceil(((t - y0) / 864e5 + 1) / 7);
  return `${t.getUTCFullYear()}-W${pad(w)}`;
}

const shortDate = (d) => d.toLocaleDateString('pt-BR', { day: 'numeric', month: 'short' }).replace('.', '');

/* ---------- Estado ---------- */

function defaults() {
  return {
    version: 1,
    vision: { antiVision: '', vision: '', yearGoal: '', monthProject: '', constraints: '' },
    levers: DEFAULT_LEVERS.map((text) => ({ id: uid(), text })),
    weeklySteps: DEFAULT_STEPS.map((text) => ({ id: uid(), text })),
    days: {},
    weeks: {}
  };
}

function normalize(s) {
  const def = defaults();
  if (!s || typeof s !== 'object') return def;
  return {
    ...def,
    ...s,
    vision: { ...def.vision, ...(s.vision || {}) },
    levers: Array.isArray(s.levers) ? s.levers : def.levers,
    weeklySteps: Array.isArray(s.weeklySteps) ? s.weeklySteps : def.weeklySteps,
    days: s.days && typeof s.days === 'object' ? s.days : {},
    weeks: s.weeks && typeof s.weeks === 'object' ? s.weeks : {}
  };
}

function load() {
  try { return normalize(JSON.parse(localStorage.getItem(KEY))); } catch (e) { return defaults(); }
}

let state = load();
let saveTimer = null;
let dirty = false;

function save() {
  clearTimeout(saveTimer);
  dirty = false;
  try {
    localStorage.setItem(KEY, JSON.stringify(state));
  } catch (e) {
    toast('Não foi possível salvar neste navegador');
  }
}

function saveSoon() {
  dirty = true;
  clearTimeout(saveTimer);
  saveTimer = setTimeout(save, 300);
}

function day(k) {
  if (!state.days[k]) {
    state.days[k] = { done: {}, priorities: [0, 1, 2].map(() => ({ text: '', done: false })), energy: 0, note: '' };
  }
  return state.days[k];
}

function week(k) {
  if (!state.weeks[k]) state.weeks[k] = { done: {}, top: ['', '', ''], wins: '', lessons: '', change: '' };
  return state.weeks[k];
}

function dayRatio(k) {
  const d = state.days[k];
  const L = state.levers;
  if (!d || !L.length) return 0;
  return L.filter((l) => d.done[l.id]).length / L.length;
}

/* ---------- Navegação ---------- */

const app = document.getElementById('app');
const titleEl = document.getElementById('title');

let tab = location.hash.slice(1) || 'hoje';
let viewDate = new Date();
let viewWeek = new Date();
let editSteps = false;

function render() {
  if (!TITLES[tab]) tab = 'hoje';
  titleEl.textContent = TITLES[tab];
  document.querySelectorAll('.tabs a').forEach((a) => {
    if (a.dataset.tab === tab) a.setAttribute('aria-current', 'page');
    else a.removeAttribute('aria-current');
  });
  const views = { hoje: renderHoje, visao: renderVisao, semana: renderSemana, progresso: renderProgresso };
  app.innerHTML = views[tab]();
}

window.addEventListener('hashchange', () => {
  tab = location.hash.slice(1) || 'hoje';
  render();
  window.scrollTo(0, 0);
});

/* ---------- Hoje ---------- */

function renderHoje() {
  const k = dkey(viewDate);
  const today = dkey(new Date());
  const d = day(k);
  const v = state.vision;
  const total = state.levers.length;
  const done = state.levers.filter((l) => d.done[l.id]).length;
  const pct = total ? Math.round((done / total) * 100) : 0;
  const n = Math.round(parseKey(k).getTime() / 864e5);
  const prompt = PROMPTS[((n % PROMPTS.length) + PROMPTS.length) % PROMPTS.length];
  const wk = state.weeks[isoWeek(viewDate)];
  const weekTop = wk ? wk.top.filter((t) => t.trim()) : [];

  const hasFocus = v.monthProject || v.constraints || weekTop.length;
  const focus = hasFocus
    ? `<section class="card focus">
        ${v.monthProject ? `<div class="label">Projeto do mês</div><p class="pre">${esc(v.monthProject)}</p>` : ''}
        ${weekTop.length ? `<div class="label">Prioridades da semana</div><p>${weekTop.map(esc).join(' · ')}</p>` : ''}
        ${v.constraints ? `<div class="label">Restrições</div><p class="pre">${esc(v.constraints)}</p>` : ''}
      </section>`
    : `<section class="card focus"><p>Comece pela aba <a href="#visao">Visão</a>: antivisão, visão, meta de 1 ano e projeto do mês. Depois ajuste suas alavancas diárias.</p></section>`;

  return `
    <div class="daynav">
      <button class="icon-btn" data-act="day" data-n="-1" aria-label="Dia anterior">‹</button>
      <div class="daylabel">
        <strong>${esc(cap(viewDate.toLocaleDateString('pt-BR', { weekday: 'long', day: 'numeric', month: 'long' })))}</strong>
        ${k !== today ? '<button class="link" data-act="today">Voltar para hoje</button>' : ''}
      </div>
      <button class="icon-btn" data-act="day" data-n="1" aria-label="Próximo dia" ${k >= today ? 'disabled' : ''}>›</button>
    </div>

    ${focus}

    <section class="card">
      <div class="card-head"><h2>Alavancas diárias</h2><span class="count">${done}/${total}</span></div>
      <div class="bar" role="progressbar" aria-valuemin="0" aria-valuemax="100" aria-valuenow="${pct}"><span style="width:${pct}%"></span></div>
      ${total
        ? `<ul class="checks">${state.levers.map((l) => `
            <li><label class="check">
              <input type="checkbox" data-act="lever" data-id="${esc(l.id)}" ${d.done[l.id] ? 'checked' : ''}>
              <span>${esc(l.text)}</span>
            </label></li>`).join('')}</ul>`
        : '<p class="muted">Nenhuma alavanca ainda. <a href="#visao">Adicionar alavancas</a></p>'}
    </section>

    <section class="card">
      <h2>3 prioridades do dia</h2>
      <ul class="pris">${d.priorities.map((p, i) => `
        <li>
          <input type="checkbox" data-act="pri" data-i="${i}" ${p.done ? 'checked' : ''} aria-label="Prioridade ${i + 1} concluída">
          <input type="text" class="${p.done ? 'done' : ''}" data-field="pri" data-i="${i}" value="${esc(p.text)}" placeholder="Prioridade ${i + 1}" maxlength="140">
        </li>`).join('')}
      </ul>
    </section>

    <section class="card">
      <h2>Energia</h2>
      <div class="scale" role="group" aria-label="Nível de energia de 1 a 5">
        ${[1, 2, 3, 4, 5].map((e) => `<button data-act="energy" data-v="${e}" aria-pressed="${d.energy === e}">${e}</button>`).join('')}
      </div>
      <div class="scale-legend"><span>Esgotado</span><span>No topo</span></div>
    </section>

    <section class="card">
      <h2>Pergunta do dia</h2>
      <p class="prompt">${esc(prompt)}</p>
      <textarea data-field="note" rows="4" placeholder="Escreva livremente…" aria-label="Resposta da pergunta do dia">${esc(d.note)}</textarea>
    </section>`;
}

/* ---------- Visão ---------- */

function renderVisao() {
  const fields = VISION_FIELDS.map(([f, label, hint, rows]) => `
    <section class="card">
      <h2><label for="v-${f}">${label}</label></h2>
      <p class="hint">${hint}</p>
      <textarea id="v-${f}" data-field="vision.${f}" rows="${rows}">${esc(state.vision[f])}</textarea>
    </section>`);

  const levers = `
    <section class="card">
      <h2>Alavancas diárias</h2>
      <p class="hint">Ações pequenas, feitas todo dia, que movem o projeto do mês. Elas viram o checklist da aba Hoje.</p>
      <ul class="editlist">${state.levers.map((l, i) => `
        <li>
          <span>${esc(l.text)}</span>
          <button class="icon-btn sm" data-act="lever-up" data-i="${i}" aria-label="Mover para cima" ${i === 0 ? 'disabled' : ''}>↑</button>
          <button class="icon-btn sm danger" data-act="del-lever" data-id="${esc(l.id)}" aria-label="Remover">✕</button>
        </li>`).join('')}
      </ul>
      <form class="addrow" data-form="add-lever">
        <input type="text" name="t" placeholder="Nova alavanca" maxlength="120" required aria-label="Nova alavanca">
        <button class="btn">Adicionar</button>
      </form>
    </section>`;

  // Ordem do protocolo: antivisão, visão, meta de 1 ano, projeto do mês, alavancas, restrições
  return `
    <p class="intro">Responda com calma, uma vez. Depois revise todo mês ou quando sentir que perdeu o rumo.</p>
    ${fields.slice(0, 4).join('')}
    ${levers}
    ${fields[4]}
    <details class="card">
      <summary>Como usar o método</summary>
      <ol>
        <li><b>Antivisão</b>: o que você quer evitar. Dá a urgência.</li>
        <li><b>Visão</b>: para onde você quer ir. Dá a direção.</li>
        <li><b>Meta de 1 ano</b>: o marco que prova o avanço.</li>
        <li><b>Projeto do mês</b>: o próximo passo concreto.</li>
        <li><b>Alavancas diárias</b>: o que você faz todo dia para o projeto andar.</li>
        <li><b>Restrições</b>: o que você não faz, para proteger o foco.</li>
        <li>Todo domingo, faça o <b>Reset semanal</b>: revise, organize e escolha as 3 prioridades.</li>
      </ol>
    </details>`;
}

/* ---------- Semana ---------- */

function renderSemana() {
  const ws = weekStart(viewWeek);
  const we = addDays(ws, 6);
  const wk = isoWeek(ws);
  const w = week(wk);
  const isCurrent = dkey(ws) === dkey(weekStart(new Date()));
  const todayK = dkey(new Date());

  let sum = 0, n = 0, eSum = 0, eN = 0;
  for (let i = 0; i < 7; i++) {
    const k = dkey(addDays(ws, i));
    if (k > todayK) break;
    sum += dayRatio(k); n++;
    const e = state.days[k]?.energy;
    if (e) { eSum += e; eN++; }
  }
  const leverPct = n ? Math.round((sum / n) * 100) : 0;
  const steps = state.weeklySteps;
  const stepsDone = steps.filter((s) => w.done[s.id]).length;

  const stepList = editSteps
    ? `<ul class="editlist">${steps.map((s) => `
        <li><span>${esc(s.text)}</span>
          <button class="icon-btn sm danger" data-act="del-step" data-id="${esc(s.id)}" aria-label="Remover">✕</button></li>`).join('')}
      </ul>
      <form class="addrow" data-form="add-step">
        <input type="text" name="t" placeholder="Novo passo" maxlength="120" required aria-label="Novo passo">
        <button class="btn">Adicionar</button>
      </form>`
    : `<ul class="checks">${steps.map((s) => `
        <li><label class="check">
          <input type="checkbox" data-act="step" data-id="${esc(s.id)}" ${w.done[s.id] ? 'checked' : ''}>
          <span>${esc(s.text)}</span>
        </label></li>`).join('')}
      </ul>`;

  return `
    <div class="daynav">
      <button class="icon-btn" data-act="week" data-n="-1" aria-label="Semana anterior">‹</button>
      <div class="daylabel">
        <strong>${shortDate(ws)} – ${shortDate(we)}</strong>
        ${isCurrent ? '<span class="muted">Esta semana</span>' : '<button class="link" data-act="this-week">Ir para esta semana</button>'}
      </div>
      <button class="icon-btn" data-act="week" data-n="1" aria-label="Próxima semana" ${isCurrent ? 'disabled' : ''}>›</button>
    </div>

    <div class="stats">
      <div class="stat"><b>${leverPct}%</b><span>alavancas</span></div>
      <div class="stat"><b>${eN ? (eSum / eN).toFixed(1).replace('.', ',') : '–'}</b><span>energia média</span></div>
      <div class="stat"><b>${stepsDone}/${steps.length}</b><span>reset</span></div>
    </div>

    <section class="card">
      <h2>3 prioridades da semana</h2>
      <ul class="toplist">${w.top.map((t, i) => `
        <li><input type="text" data-field="weektop" data-i="${i}" value="${esc(t)}" placeholder="Prioridade ${i + 1}" maxlength="140" aria-label="Prioridade da semana ${i + 1}"></li>`).join('')}
      </ul>
    </section>

    <section class="card">
      <div class="card-head">
        <h2>Checklist do reset</h2>
        <button class="link" data-act="edit-steps">${editSteps ? 'Concluir edição' : 'Editar'}</button>
      </div>
      ${stepList}
    </section>

    <section class="card reflect">
      <h2>Revisão da semana</h2>
      <label for="w-wins">O que deu certo?</label>
      <textarea id="w-wins" data-field="week" data-k="wins" rows="3">${esc(w.wins)}</textarea>
      <label for="w-lessons">O que não funcionou?</label>
      <textarea id="w-lessons" data-field="week" data-k="lessons" rows="3">${esc(w.lessons)}</textarea>
      <label for="w-change">O que vou mudar na próxima semana?</label>
      <textarea id="w-change" data-field="week" data-k="change" rows="3">${esc(w.change)}</textarea>
    </section>`;
}

/* ---------- Progresso ---------- */

function streaks() {
  const today = new Date();
  let cur = 0;
  let i = dayRatio(dkey(today)) === 1 ? 0 : 1; // o dia de hoje ainda está em andamento
  while (dayRatio(dkey(addDays(today, -i))) === 1) { cur++; i++; }

  let best = 0, run = 0;
  const keys = Object.keys(state.days).sort();
  if (keys.length) {
    const end = dkey(today);
    for (let d = parseKey(keys[0]); dkey(d) <= end; d = addDays(d, 1)) {
      if (dayRatio(dkey(d)) === 1) { run++; best = Math.max(best, run); } else run = 0;
    }
  }
  return { cur, best: Math.max(best, cur) };
}

function level(r) {
  if (r <= 0) return 0;
  if (r < 0.34) return 1;
  if (r < 0.67) return 2;
  if (r < 1) return 3;
  return 4;
}

function renderProgresso() {
  const today = new Date();
  const todayK = dkey(today);
  const { cur, best } = streaks();

  let s30 = 0;
  for (let i = 0; i < 30; i++) s30 += dayRatio(dkey(addDays(today, -i)));

  const start = addDays(weekStart(today), -7 * 11);
  const cells = [];
  for (let i = 0; i < 84; i++) {
    const d = addDays(start, i);
    const k = dkey(d);
    if (k > todayK) { cells.push('<i class="future"></i>'); continue; }
    const r = dayRatio(k);
    const label = `${d.toLocaleDateString('pt-BR')}: ${Math.round(r * 100)}%`;
    cells.push(`<i class="l${level(r)}${k === todayK ? ' today' : ''}" title="${label}" aria-label="${label}"></i>`);
  }

  const leverBars = state.levers.map((l) => {
    let c = 0;
    for (let i = 0; i < 30; i++) if (state.days[dkey(addDays(today, -i))]?.done[l.id]) c++;
    const p = Math.round((c / 30) * 100);
    return `<li><div class="row"><span>${esc(l.text)}</span><span class="muted">${c}/30</span></div>
      <div class="bar"><span style="width:${p}%"></span></div></li>`;
  }).join('');

  return `
    <div class="stats">
      <div class="stat"><b>${cur}</b><span>dias seguidos</span></div>
      <div class="stat"><b>${best}</b><span>recorde</span></div>
      <div class="stat"><b>${Math.round((s30 / 30) * 100)}%</b><span>últimos 30 dias</span></div>
    </div>

    <section class="card">
      <h2>Últimas 12 semanas</h2>
      <div class="heat">
        ${['S', 'T', 'Q', 'Q', 'S', 'S', 'D'].map((x) => `<span class="dow">${x}</span>`).join('')}
        ${cells.join('')}
      </div>
      <div class="legend"><span>0%</span><i></i><i class="l1"></i><i class="l2"></i><i class="l3"></i><i class="l4"></i><span>100%</span></div>
    </section>

    <section class="card">
      <h2>Alavancas nos últimos 30 dias</h2>
      ${leverBars ? `<ul class="bars">${leverBars}</ul>` : '<p class="muted">Nenhuma alavanca cadastrada.</p>'}
    </section>

    <section class="card">
      <h2>Instalar no celular</h2>
      <p class="hint" style="margin:0">Android (Chrome): menu ⋮ → <b>Instalar app</b>.<br>iPhone (Safari): Compartilhar → <b>Adicionar à Tela de Início</b>.</p>
    </section>

    <section class="card">
      <h2>Backup</h2>
      <p class="hint">Seus dados ficam só neste aparelho. Exporte de vez em quando para não perder nada.</p>
      <div class="actions">
        <button class="btn" data-act="export">Exportar</button>
        <label class="btn ghost">Importar<input type="file" accept="application/json,.json" data-act="import" class="sr-only"></label>
        <button class="btn danger" data-act="wipe">Apagar tudo</button>
      </div>
    </section>`;
}

/* ---------- Eventos ---------- */

app.addEventListener('click', (e) => {
  const el = e.target.closest('button[data-act]');
  if (!el) return;
  const act = el.dataset.act;

  switch (act) {
    case 'day':
      viewDate = addDays(viewDate, Number(el.dataset.n));
      break;
    case 'today':
      viewDate = new Date();
      break;
    case 'energy': {
      const d = day(dkey(viewDate));
      const v = Number(el.dataset.v);
      d.energy = d.energy === v ? 0 : v;
      save();
      break;
    }
    case 'lever-up': {
      const i = Number(el.dataset.i);
      if (i > 0) [state.levers[i - 1], state.levers[i]] = [state.levers[i], state.levers[i - 1]];
      save();
      break;
    }
    case 'del-lever': {
      const l = state.levers.find((x) => x.id === el.dataset.id);
      if (!l || !confirm(`Remover a alavanca "${l.text}"?`)) return;
      state.levers = state.levers.filter((x) => x.id !== l.id);
      save();
      break;
    }
    case 'week':
      viewWeek = addDays(viewWeek, 7 * Number(el.dataset.n));
      break;
    case 'this-week':
      viewWeek = new Date();
      break;
    case 'edit-steps':
      editSteps = !editSteps;
      break;
    case 'del-step': {
      const s = state.weeklySteps.find((x) => x.id === el.dataset.id);
      if (!s || !confirm(`Remover o passo "${s.text}"?`)) return;
      state.weeklySteps = state.weeklySteps.filter((x) => x.id !== s.id);
      save();
      break;
    }
    case 'export':
      exportData();
      return;
    case 'wipe':
      if (!confirm('Apagar todos os dados deste aparelho? Isso não pode ser desfeito.')) return;
      state = defaults();
      save();
      toast('Dados apagados');
      break;
    default:
      return;
  }
  render();
});

app.addEventListener('change', (e) => {
  const el = e.target;
  const act = el.dataset.act;
  if (act === 'lever') {
    day(dkey(viewDate)).done[el.dataset.id] = el.checked;
  } else if (act === 'pri') {
    day(dkey(viewDate)).priorities[Number(el.dataset.i)].done = el.checked;
  } else if (act === 'step') {
    week(isoWeek(weekStart(viewWeek))).done[el.dataset.id] = el.checked;
  } else if (act === 'import') {
    importData(el.files[0]);
    return;
  } else {
    return;
  }
  save();
  render();
});

app.addEventListener('input', (e) => {
  const el = e.target;
  const f = el.dataset.field;
  if (!f) return;
  const v = el.value;
  if (f === 'pri') day(dkey(viewDate)).priorities[Number(el.dataset.i)].text = v;
  else if (f === 'note') day(dkey(viewDate)).note = v;
  else if (f === 'week') week(isoWeek(weekStart(viewWeek)))[el.dataset.k] = v;
  else if (f === 'weektop') week(isoWeek(weekStart(viewWeek))).top[Number(el.dataset.i)] = v;
  else if (f.startsWith('vision.')) state.vision[f.slice(7)] = v;
  else return;
  saveSoon();
});

app.addEventListener('submit', (e) => {
  const form = e.target.closest('form[data-form]');
  if (!form) return;
  e.preventDefault();
  const text = form.elements.t.value.trim();
  if (!text) return;
  const kind = form.dataset.form;
  if (kind === 'add-lever') state.levers.push({ id: uid(), text });
  else if (kind === 'add-step') state.weeklySteps.push({ id: uid(), text });
  save();
  render();
  const input = app.querySelector(`form[data-form="${kind}"] input`);
  if (input) input.focus();
});

/* ---------- Backup ---------- */

function exportData() {
  save();
  const blob = new Blob([JSON.stringify(state, null, 2)], { type: 'application/json' });
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = `life-reset-${dkey(new Date())}.json`;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(a.href), 1000);
}

function importData(file) {
  if (!file) return;
  const reader = new FileReader();
  reader.onload = () => {
    let data;
    try { data = JSON.parse(reader.result); } catch (e) { data = null; }
    if (!data || !Array.isArray(data.levers) || typeof data.days !== 'object') {
      toast('Arquivo inválido');
      return;
    }
    if (!confirm('Substituir os dados deste aparelho pelos do arquivo?')) return;
    state = normalize(data);
    save();
    render();
    toast('Backup importado');
  };
  reader.readAsText(file);
}

/* ---------- Toast ---------- */

let toastTimer;
function toast(msg) {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { t.hidden = true; }, 2500);
}

/* ---------- Ciclo de vida ---------- */

let lastSeenDay = dkey(new Date());

document.addEventListener('visibilitychange', () => {
  if (document.hidden) { if (dirty) save(); return; }
  // Se o app ficou aberto de um dia para o outro, volta para o dia atual.
  const now = dkey(new Date());
  if (now !== lastSeenDay) {
    if (dkey(viewDate) === lastSeenDay) viewDate = new Date();
    lastSeenDay = now;
    render();
  }
});
window.addEventListener('pagehide', () => { if (dirty) save(); });

// Outra aba do app salvou: recarrega para não sobrescrever dados mais novos.
window.addEventListener('storage', (e) => {
  if (e.key !== KEY || dirty) return;
  state = load();
  render();
});

if ('serviceWorker' in navigator && (location.protocol === 'https:' || location.hostname === 'localhost')) {
  navigator.serviceWorker.register('sw.js').catch(() => {});
}

render();
