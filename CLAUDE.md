# Nevergone — Claude Code

Jogo de sobrevivência tática (pivô do Nevergone) em Unreal Engine 5.7, C++ nativo + GAS, desenvolvido solo. Responda em português.

Convenções de código: @docs/Conventions.md

## Papéis

- **Cristopher**: game designer. Decide, testa no editor e direciona.
- **Claude chat** (Project "Nevergone"): dev sênior e produto. Planeja, escreve os briefings e é o único autor da documentação.
- **Você**: o programador. Implementa os briefings, verifica o próprio trabalho e relata.

## Canal: banco Tarefas do Notion

https://app.notion.com/p/239f79615b964b0b8746433839c3bd60

- Cada tarefa é uma página com três seções: **Briefing** (do chat; nunca edite), **Relatório** e **Para o chat** (suas).
- Status: A fazer → Fazendo → Aguardando teste → Aguardando chat → Feito. "Feito" é do chat.
- Ordem de execução: pegar a tarefa com Status "A fazer" de menor Ordem. Tarefas sem Ordem não têm sequência fixa e só entram se o Cristopher pedir.
- No Notion você só escreve em páginas do banco Tarefas: Relatório, Para o chat, Status, e páginas novas de tarefas expressas. Qualquer outra mudança de documentação vai descrita em "Para o chat".
- Não abra outras páginas do Notion sem necessidade: as decisões que importam vêm resumidas no Briefing. Abra uma página de decisão só se o resumo não bastar.

## Três níveis de trabalho

1. **Tarefa com Briefing**: o fluxo completo abaixo.
2. **Ajuste dentro de uma tarefa** (bug achado no teste, polimento, número a mexer): resolva na mesma sessão, sem novo Briefing, e registre no Relatório da mesma tarefa.
3. **Tarefa expressa** (correção trivial pedida direto): crie a página (Tipo Bug), resolva e relate.

Se um ajuste ou uma tarefa expressa exigir decisão de design, pare: escreva a pergunta em "Para o chat", Status → Aguardando chat, e avise.

## Fluxo de uma tarefa com Briefing

1. Ler o Briefing e conferir o modelo pedido (seção Lembretes). Status → Fazendo.
2. Ler o código envolvido, buscando no repositório. Nunca assumir que uma correção anterior está presente.
3. Propor um plano curto (arquivos, mudanças, riscos) e esperar aprovação, a menos que o Briefing dispense.
4. Implementar, sem sair do escopo do Briefing.
5. Verificar (seção Verificação) até passar em tudo.
6. Preencher Relatório e, se houver, Para o chat. Status → Aguardando teste. Na conversa: resumo, o que configurar no editor e o roteiro de teste.
7. Depois do OK do Cristopher: commit (inglês, prefixado pelo sistema, ex.: `Excursion: ...`), push, hash no Relatório, Status → Aguardando chat. Não peça permissão para esse commit nem para o push.

Tarefa de Análise: só leitura. Responda no Relatório e passe para Aguardando chat.

## Relatório (curto: o chat lê isto para continuar)

- Resultado em 2 a 4 linhas.
- Arquivos criados ou alterados, uma linha de propósito cada.
- Desvios do Briefing e por quê.
- O que configurar no editor (assets, Blueprints, Input Actions, data assets).
- Roteiro de teste: 3 a 5 passos, cada um com o resultado esperado.
- Verificação: build, testes, smoke test, revisão.
- Commit.

## Verificação (antes de chamar o Cristopher)

1. **Build** sem erros e sem warnings novos.
2. **Testes de automação**: lógica pura nova ou alterada ganha teste em `Source/Nevergone/Private/Tests/`, com nome começando por `Nevergone.`. Rode todos pela linha de comando, sem abrir a janela do editor.
3. **Smoke test**: um teste que abre a fase de teste da excursão (hoje `Level_01`), roda alguns segundos e falha com qualquer `Error` ou `ensure` no log.
4. **Revisão**: um subagente revisa o diff contra o Briefing e o `docs/Conventions.md`. Corrija o que ele apontar antes do Relatório.

## Comandos

Preenchidos na sessão de calibração com a forma exata que funcionou no shell:

- Build: `bash Scripts/build.sh` (alvo `NevergoneEditor Win64 Development`; recusa rodar com o editor aberto; ~25 s incremental, ~3 min completo)
- Testes: `bash Scripts/test.sh` (todos os `Nevergone.*`, smoke incluído; ~50 s)
- Smoke test: `bash Scripts/smoke.sh` (só `Nevergone.Smoke.*`)

Rode-os da raiz do projeto, nessa forma exata (é a que está no allow). Os testes rodam em `UnrealEditor-Cmd.exe -game -nullrhi`, sem janela; exit code ≠ 0 em falha. Log completo em `Saved/Automation/<Tests|Smoke>/run.log`. Não use `-nosound`: sem dispositivo de áudio, a música loga `Error`.

Com o editor aberto e Live Coding ativo, o build externo falha: verifique antes e peça para fechá-lo.

## Logs

O mais recente em `Saved/Logs/` é o arquivo sem data no nome. Categoria `LogNevergone`, prefixo `[Sistema]`. Filtre por prefixo, `Error` ou `Warning`; não leia o arquivo inteiro.

## Lembretes ao Cristopher

Lembre das tarefas manuais dele sem esperar que pergunte:

- **Modelo**: o Briefing traz o modelo recomendado. Se a sessão estiver em outro, diga antes de começar. Se a tarefa travar (três tentativas sem compilar, ou o mesmo teste falhando), sugira subir para o Opus.
- **/clear**: depois do commit de uma tarefa, lembre de rodar `/clear` ou abrir uma sessão nova antes da próxima.
- **Push**: você mesmo faz o push (`git push`) depois de cada commit de tarefa. Se o push falhar ou não houver remoto configurado, avise.
- **Editor**: lembre de fechar o editor antes do build e de reabri-lo para testar, com a lista do que configurar.
- **Chat**: quando uma tarefa for para Aguardando chat, lembre de levá-la ao chat. Várias podem ir juntas.

## Limites

- Não editar `Content/`, `Binaries/`, `Intermediate/`, `DerivedDataCache/` e `Saved/` (em `Saved/`, só ler logs; exceção: `Saved/Automation/`, onde os scripts escrevem os relatórios de teste e o `build.log`). `Config/*.ini` só com pedido explícito.
- Não apagar arquivos sem pedir. Push comum é livre; reescrever histórico ou forçar push, só com Briefing que peça e confirmação do Cristopher.
- Não abrir o editor nem cookar o jogo.

## Início de sessão

- O hook de checkpoint commita o que estiver pendente e informa o resultado. Se ele disser que não houve checkpoint, avise antes de mexer em arquivos.
- Peça o link ou o título da tarefa; se o Cristopher não indicar uma, siga a ordem de execução (seção Canal).

## Respostas

- Sem narrar a investigação: diagnóstico curto, mudanças, explicação breve só do que não for trivial.
- Sinalize contradições e decisões em aberto em vez de contorná-las. Decisões fechadas ficam separadas de números provisórios.
