# Life Reset

App web para organizar a rotina, combinando dois métodos:

- **Protocolo de reset de vida**: antivisão, visão, meta de 1 ano, projeto do mês, alavancas diárias e restrições.
- **Reset semanal**: checklist de domingo, revisão da semana e 3 prioridades.

Funciona no celular, pode ser instalado na tela inicial e funciona offline. Os dados ficam salvos só no aparelho (localStorage), sem servidor e sem conta.

## Abas

| Aba | O que tem |
| --- | --- |
| **Hoje** | Projeto do mês, prioridades da semana e restrições em destaque; rotina do dia com horários (destaca o bloco atual); checklist das alavancas diárias; 3 prioridades do dia; energia (1–5); pergunta do dia com espaço para escrever. Dá para navegar pelos dias anteriores. |
| **Visão** | Antivisão, visão, meta de 1 ano, projeto do mês, restrições e o editor das alavancas diárias. |
| **Semana** | 3 prioridades da semana, checklist do reset (editável), revisão (o que deu certo, o que não funcionou, o que mudar) e resumo da semana. |
| **Progresso** | Dias seguidos com todas as alavancas feitas, recorde, mapa das últimas 12 semanas, desempenho de cada alavanca em 30 dias, backup (exportar e importar JSON). |

## Publicar no GitHub Pages

1. No repositório: **Settings → Pages**.
2. Em *Build and deployment*, escolha **Deploy from a branch**, branch `main`, pasta `/ (root)`.
3. Depois de 1–2 minutos o app fica em `https://<seu-usuario>.github.io/<nome-do-repo>/`.
4. Abra esse link no celular e instale:
   - Android (Chrome): menu ⋮ → **Instalar app**
   - iPhone (Safari): Compartilhar → **Adicionar à Tela de Início**

## Rodar localmente

Qualquer servidor estático serve, por exemplo:

```bash
python3 -m http.server 8000
```

Depois abra `http://localhost:8000`.

## Backup

Como os dados ficam só no navegador do aparelho, use **Progresso → Exportar** de vez em quando. Para levar os dados para outro aparelho, use **Importar** com o arquivo exportado.

## Estrutura

```
index.html            página e navegação
styles.css            visual (tema claro e escuro automáticos)
app.js                lógica e dados
sw.js                 service worker (offline)
manifest.webmanifest  instalação como app
icons/                ícones
```
