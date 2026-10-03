# Context Pack — Novo Jogo (pivô do Nevergone)

> **O que é este arquivo.** Contexto de sessão para o Claude. Traz o conceito, as decisões fechadas, o escopo e as convenções técnicas que valem em toda conversa. O que não está aqui está no Notion (links no fim).
>
> **Hierarquia de verdade:** código enviado > Notion > este arquivo. Ao encontrar divergência, sinalizar na mesma resposta.
>
> **Nome do jogo:** ainda não definido.

---

## 1 — Conceito

Um grupo de sobreviventes atravessa cidades num apocalipse zumbi em busca da cura. De dia, um sobrevivente escolhido faz excursões furtivas em tempo real para coletar recursos e encontrar outros sobreviventes. À noite, todos defendem um abrigo em combate tático por turnos.

**Diferencial:** exploração stealth em tempo real (sem combate) combinada com defesa tática por turnos, ligadas por recursos, sobreviventes e pela pesquisa da cura.

**Loop:**
```
Chegada numa cidade
  → Dia: alocar sobreviventes em atividades
      → um sobrevivente faz a excursão (stealth, tempo real)
      → outros: pesquisa, fortificação, veículo, fabricação
  → Noite: todos defendem o abrigo (tático por turnos)
  → Repetir até a cidade se esgotar ou a pressão ficar alta
  → Viajar para a próxima cidade (rota guiada pela pesquisa)
Fim: cura concluída (vitória) ou todos os sobreviventes mortos (derrota)
```

**Referências:** The Last Stand 2 (loop dia/noite, travessia), Mutant Year Zero (tempo real + turnos), The Last Spell (defesa tática noturna), Into the Breach (inimigos simples com intenção visível), FTL (jornada com fim definido).

---

## 2 — Decisões fechadas

### Estrutura
1. Sem protagonista; o jogador gerencia um grupo de sobreviventes.
2. Morte permanente dos sobreviventes.
3. Campanha com fim definido: busca pela cura, atravessando cidades.
4. Abrigo novo a cada cidade + veículo com upgrades persistentes.

### Dia — excursão
5. Um sobrevivente escolhido, em 3ª pessoa, tempo real.
6. Stealth simples: zumbis parados vigiando corredores ou andando devagar; rotas alternativas e itens de distração.
7. **Sem combate jogável de dia.** O único jeito de matar um zumbi é a captura com item de safeguard, resolvida por animação. *Substitui "sem combate de dia".*
8. **Captura resolvida sem escolha.** Com item de safeguard (ex.: pé de cabra), o item é usado sozinho e quebra; o sobrevivente mata o zumbi e sai ileso. Sem o item, uma rolagem decide entre mordida (ele empurra o zumbi ao chão e ganha alguns segundos para escapar) e morte (o zumbi o derruba e o devora). Nova captura depois de uma mordida, na mesma excursão, rola de novo com chance maior de morte. *Substitui o encontro com tela de três opções (gastar recurso, lutar, fugir).*
9. A rolagem usa os **atributos do sobrevivente** e a chance base do data asset de cada tipo de zumbi; nunca é combate jogável — a decisão 7 continua valendo.
10. **Infecção é consequência da captura, não sistema do P0.** No P0 a captura só emite o resultado (ileso / infectado / morto); o efeito entra no P2, junto com o ciclo.
11. Resultado da excursão afeta a noite: buff (bons recursos ou sobrevivente encontrado) ou debuff (ferimento). O safeguard quebra **sem debuff**. *Substitui "salvar com item gera item gasto e debuff".*
12. **Janela de rastreamento na perseguição.** Ao perder contato durante uma perseguição, o zumbi sabe a posição real do sobrevivente por uma janela curta, por tipo (`TrackingGraceSeconds`). Vale só durante a perseguição; quem ainda não confirmou o sobrevivente nunca rastreia através de paredes.
13. **Ruído em três camadas:** fonte (`UNoiseDefinition`: tag, loudness de IA e som), superfície (material físico multiplica o loudness e troca o som) e caminho até quem ouve (oclusão e mascaramento, por ouvinte). Emissão por uma única porta, o `UExcursionNoiseSubsystem`; som do jogador e ruído da IA saem do mesmo evento, com alcances independentes.
14. **Oclusão por parede:** cada objeto entre o zumbi e o ruído multiplica o alcance por um fator do tipo de zumbi, uma vez por objeto. Objeto encostado na fonte não conta.
15. **Mascaramento ambiental:** volumes (chuva, gerador) reduzem o alcance da audição. Vale a ponta mais barulhenta entre zumbi e ruído; sobrepostos, vence o mascaramento mais forte. Encolhe alcance, não apaga o ruído.
16. **Objetos derrubáveis:** esbarrar acima de uma velocidade derruba; agachado nunca derruba. Ruído a cada impacto forte, com intervalo mínimo; derrubar de novo exige sair e voltar ao objeto.
17. **Correr e agachar:** três velocidades. Agachar alterna por padrão, com segurar/alternar configurável nas opções. Correr passa do limite que derruba objetos; andar não.
18. **Fôlego é só da exploração:** vive no `UExplorationModeComponent`, enche a cada excursão, não é salvo. Ao zerar não há penalidade de velocidade; o sobrevivente fica ofegante e emite ruído por alguns segundos.
19. **Excursão em sessão única:** save automático na entrada do prédio, nenhum save durante a excursão, só no retorno à base. Estado volátil (objetos derrubados) não é salvo. Substitui o save-anywhere herdado.
20. **Captura congela o mundo só quando a vítima é o jogador:** só ele e o zumbi se movem, para ele não sair de uma captura direto para outra. Na captura do seguidor, o mundo continua.
21. **Sobrevivente a resgatar:** encontrado no prédio; uma conversa curta pelo diálogo existente pode recrutá-lo para seguir o jogador. Depois de recrutado, interagir com ele alterna entre seguir e esperar, sem diálogo.
22. **O seguidor imita o jogador:** agacha e corre junto, com as mesmas velocidades e fôlego próprio (esgota e ofega também). Não corre para alcançar; só encurta a distância quando o jogador para. Passos, ofegar e objetos que ele derruba atraem zumbis.
23. **Zumbis percebem o seguidor** por ruído, visão e cheiro desde o recrutamento, inclusive esperando. A captura mata o seguidor; o zumbi então investiga o último ponto onde percebeu o jogador, por mais antigo que seja (desistir de uma busca não apaga essa memória), ou volta ao normal se nunca o percebeu. A decisão 12 continua valendo.
24. **Resgate:** sair da excursão com o sobrevivente seguindo o coloca na party, na base. Se o jogador morre, o seguidor morre junto. No P0 a excursão só emite o resultado; a entrada na party é aplicada no P2.
25. **Diálogo e telas da excursão pausam o mundo:** com diálogo, inventário ou contêiner aberto, nada se move; um zumbi não alcança o jogador no meio de uma conversa ou de um saque.
26. **Recursos só ficam com o grupo na saída:** se o jogador morre na excursão, o que coletou nela é perdido; o que foi gasto continua gasto.
27. **Saída pela porta:** a excursão termina ao interagir com a porta de saída. Só conta como resgatado quem está seguindo nesse momento.
28. **Item de distração:** segurar mira com o arco visível; soltar arremessa. Ruído no primeiro impacto, atribuído ao jogador; o objeto fica no chão para ser pego de novo.

### Noite — defesa
29. Combate tático por turnos no abrigo.
30. Todos os sobreviventes participam, independentemente da atividade do dia.
31. Não precisa ser horda: pode ser menos zumbis e mais fortes.
32. Zumbis com regras simples (avançam no alvo mais próximo); twists vêm dos tipos de inimigo.
33. O planner de IA atual será refeito.

### Ligação e pressão
34. Recursos e sobreviventes ligam os dois modos com o mesmo peso.
35. Uma excursão ruim pode custar um sobrevivente.
36. Recursos da cidade se esgotam com o tempo.
37. Dificuldade noturna em duas camadas: base global (progresso da campanha) + escalada por noite na mesma cidade.

### Pesquisa da cura
38. Avança a cada dia conforme a Inteligência do sobrevivente alocado.
39. Tiers sequenciais; cada um precisa chegar a 100% antes do próximo.
40. Cada tier exige itens específicos, sorteados por campanha e ligados a tipos de local (ex.: urânio enriquecido, tutano de girafa).
41. Os itens destravam o tier ao chegar a 100%; se encontrados antes, aceleram a pesquisa.

### Sobreviventes
42. Quatro atributos base: Força, Destreza, Inteligência, Perícia.

---

## 3 — Provisório

| Atributo | Dia | Noite |
|---|---|---|
| Força | Fortificar o abrigo | Dano corpo a corpo, empurrar/segurar |
| Destreza | Excursão (furtividade) | Dano à distância, movimento |
| Inteligência | Pesquisa da cura | Itens e armadilhas mais eficazes |
| Perícia | Reparar veículo, fabricar itens | Montar/consertar armadilhas em combate |

Rolagem da captura (provisório): o data asset do tipo de zumbi traz a chance base de morte, e o restante é mordida. Cada ponto de Força reduz a chance de morte em ~2 pontos percentuais; cada mordida anterior na excursão soma ~25 pontos. Fórmula exata vive no `UExcursionEncounterSubsystem`, não no data asset.

Velocidades: 150 agachado, 300 andando, 550 correndo; 400 é o mínimo para derrubar um objeto. Fôlego: 100 de máximo, 20/s de gasto, 12,5/s de recuperação após 1 s, mínimo de 15 para correr, ofegante por 4 s a um ruído por segundo. Audição: fator de oclusão 0,5 por parede, até 4 paredes contadas. Objeto derrubável: impacto mínimo de 150 e 0,5 s entre ruídos. Seguidor: para a 200 e volta a andar a 300; imita agachar e correr com 0,3 s de atraso. Zumbi no chão após a mordida: 4 s. Zumbi devorando o seguidor: 5 s. Arremesso: 1200 cm/s, com 10° somados ao pitch da câmera.

Números só para dimensionar: 4–5 cidades, ~3 dias por cidade, excursão de 8–10 min, noite de 10–15 min, campanha de 6–10 h, máximo de 6 sobreviventes, distância de captura ~1 m, janela de confirmação da percepção ~1,5 s, 6–8 prédios-marco.

---

## 4 — Escopo

**Milestone atual: P0 — Excursão.** Critério de saída: prédio feito à mão, um sobrevivente, um sobrevivente a resgatar que segue e imita o jogador, dois tipos de zumbi (um parado, um em patrulha), item de distração, item de safeguard, saída do prédio e captura resolvida emitindo resultado. Pergunta que responde: o stealth é divertido? Geração procedural só começa depois do P0 aprovado.

Sequência: P0 Excursão → P1 Noite → P2 Ciclo → P3 Cidade → P4 Campanha. Nenhum sistema novo fora do milestone atual.

**Fora do escopo:** combate diurno; condução do veículo em tempo real (viagem é tela de mapa); IA com planner e scoring para inimigos comuns; construção livre de base; diálogos e quests em grafo; afinidade entre NPCs; narrativa autoral pesada.

---

## 5 — Base de código herdada

Unreal Engine 5.7, C++ nativo + GAS, sem lógica em Blueprint. Módulo único `Nevergone`, com `Public/` e `Private/` espelhados. Includes escritos da raiz (`#include "ActorComponents/BattleModeComponent.h"`), nunca `Public/...`.

| Sistema existente | Uso no pivô |
|---|---|
| `UGameContextManager` (Exploration ⇄ Battle) | Ciclo dia ⇄ noite |
| `UPartyManagerSubsystem`, `SetPartyLeader`, `HandleLeaderChanged` | Grupo de sobreviventes; escolher quem faz a excursão |
| `WriteBackBattleResults` | Ferimentos e mortes entre os modos |
| Save/load (`UMySaveGame`, `USaveableComponent`, `UWorldManagerSubsystem`) | Sem save-anywhere na excursão: automático na entrada do prédio e no retorno à base |
| `UInventorySubsystem`, `AContainerActor`, `ULootTable`, `AWorldItemActor` | Coleta de recursos |
| `ADoor`, `UInteractableComponent` | Interação na excursão |
| `UGridManager`, GAS, `UTurnManager`, `UBattleState` | Defesa noturna |
| `UStatusEffectManager` | Buffs/debuffs pós-excursão |
| `FItemStatModifier` + DataAssets | Upgrades do veículo |
| `UUnitStatsComponent` | Migrar de 8 para 4 atributos |
| `UAudioSubsystem`, `ULocalizationSubsystem` | Direto |
| `ADialogueNPC`, `UDialogueSubsystem`, `UDialogueData` | Conversa curta e linear com o sobrevivente a resgatar; efeitos de quest e afinidade ficam sem uso |

**Sistemas a construir:**
- Inimigo de exploração: `AExplorationEnemyCharacter` (herda de `ACharacter`, **não** de `ACharacterBase`) + `UExplorationEnemyDefinition` por tipo + `AExplorationEnemyController` com FSM em C++ puro (Idle/Patrol → Investigate → Chase → Catch). Patrulha em três modos: parado, rota por waypoints, vagando.
- Percepção: `UAIPerceptionComponent` com `AISense_Sight` e `AISense_Hearing`; olfato como checagem de raio, já que o engine não tem o sentido. Multi-alvo: percebe e captura o jogador e o seguidor recrutado.
- `UExcursionEncounterSubsystem`: resolve a captura (safeguard ou rolagem entre mordida e morte), toca as animações pareadas da vítima e do zumbi, congela o mundo quando a vítima é o jogador e emite o resultado. Sem widget. Safeguard identificado pela tag nativa `Item.Safeguard` no `UItemDefinition`. `FightOdds`/`FleeOdds` do `UExplorationEnemyDefinition` dão lugar a uma única chance de morte.
- Sobrevivente a resgatar: `AExcursionSurvivorNPC` (subclasse de `ADialogueNPC`, portanto de `ACharacterBase`, para reaproveitar o `UExplorationModeComponent`) + `AExcursionSurvivorController` (seguir/esperar, imitação de agachar e correr).
- Item de distração: tag `Item.Distraction` + `ImpactNoise` no `UItemDefinition`; `UExcursionThrowComponent` no sobrevivente (mira com arco, alimentado pelo `UExplorationModeComponent`); `AExcursionThrowable` (só voo, com o mesh do pickup do item; ruído no primeiro impacto; ao parar vira o `WorldActorClass` do item via `UInventorySubsystem::SpawnWorldItem`).
- Saída da excursão: `AExcursionExit` (porta interagível) + `UExcursionSubsystem` (snapshot do inventário na entrada, fim pela saída ou pela morte, `FExcursionResult`) + `UExcursionResultWidget` (no P0, continuar reabre o prédio do zero).
- Ruído: `UNoiseDefinition` por tipo, `UNevergonePhysicalMaterial` por superfície, `UExcursionNoiseSubsystem` como única porta de emissão, `AExcursionNoiseMaskVolume`, `UAnimNotify_Footstep` e `AKnockableProp`. Oclusão e mascaramento filtrados por ouvinte no `UExplorationPerceptionComponent`.
- Movimento da excursão: correr com fôlego e agachar, no `UExplorationModeComponent`.
- IA noturna por regras de tipo, geração procedural de prédios, pesquisa da cura com tiers, veículo e upgrades, alocação diária, mapa de viagem.

**Débito técnico conhecido:** as condições de entry point do `ADialogueNPC` são avaliadas antes de `UDialogueSubsystem::ActiveNPC` existir, então `NPCAffinityMin` sempre falha nesse ponto. Afinidade está fora do escopo; sem uso no P0. O menu de pausa da noite (`BattlePlayerController`) não pausa o mundo; na excursão, diálogo, telas e menu de pausa já pausam. Fica para o P1.

---

## 6 — Convenções técnicas críticas

1. **`OnPostRestore` roda antes de `BeginPlay`** no carregamento de mundo. Inicialização de estado saveable vai em `OnPostRestore`; `BeginPlay` só consome.
2. **Gameplay tags nativas precisam do par:** `UE_DECLARE_GAMEPLAY_TAG_EXTERN` no header e `UE_DEFINE_GAMEPLAY_TAG` no .cpp. Nunca `RequestGameplayTag` em runtime.
3. **`SwapPlayerControllers` não destrói o controller antigo** — `Destroy()` explícito.
4. **`UPROPERTY` de GameInstanceSubsystem não é editável em Class Defaults.** Autorar em `UMyGameInstance` e espelhar no `Init()`.
5. **Autoridade de dados:** durante combate, ler HP/AP/team/status de `FBattleUnitState` via `UBattleState::FindUnitState`; fora de combate, de `FPartyMemberData` via `FindMemberMutable` + `NotifyMemberMutated`. Nunca escrever direto em `UMyGameInstance::PartyData`. Exceção: atributos base (Força etc.) não existem em `FPartyMemberData`; ler do `UUnitStatsComponent` do pawn (`Attributes` + `TemporaryAttributeBonuses`).
6. **Configuração de habilidade vive no `UAbilityDefinition`**, não na classe GA (template sem estado). Cooldowns são por definição, em `UBattleModeComponent::DefinitionCooldowns`.
7. **GUID após destroy-and-respawn:** `USaveableComponent::SetSaveIdentifier(OldId)`.
8. **Overlaps com respawn no mesmo lugar** precisam de gate de saída-antes-de-retrigger.
9. **`CaptureSnapshot` idempotente** em `UCombatPresenceComponent`.
10. **`FText::FromString` não localiza.** Usar `LOCTEXT`/`NSLOCTEXT`/`FText::Format`, FText em DataAssets ou metadata de UEnum.
11. **Shield absorve antes de mutar HP** em `NotifyDamageApplied`.
12. **Imunidade:** `Immunity.<StateTag com pontos como underscores>`.
13. **Aparência sorteada precisa ser determinística.** Variante visual de inimigo deriva do `FName` do ator (estável para atores colocados no nível), nunca de `FMath::RandRange` em runtime — senão cada load reembaralha os corpos. Ao spawnar em runtime (P3), a semente migra para valor persistido via `USaveableComponent`.

14. **Input da exploração pertence ao controller.** `AExplorationPlayerController` é dono do mapping context e das Input Actions; tudo chega ao sobrevivente pela `IExplorationInputReceiver` → `UExplorationModeComponent`. `ANevergoneCharacter` não tem input: expõe só `DoMove`, `DoJumpStart` e `DoJumpEnd`.

**Estilo:** comentários em inglês; logs `UE_LOG` com prefixo `[Sistema]` e o sujeito via `*GetNameSafe(Owner)`; `TObjectPtr` para referências possuídas, `TWeakObjectPtr` para caches entre sistemas; `TSoftObjectPtr`/`TSoftClassPtr` para o que é referenciado por save data ou por variante de arte.

---

## 7 — Como trabalhar

### Papéis

- **Chat** (Project "Nevergone" no Claude): design, arquitetura, decisões, briefings e autoria de toda a documentação (Notion e este Context Pack). Não edita código.
- **Claude Code** (pasta do projeto): implementa briefings, compila, lê logs e faz commits. Não decide design e não edita documentação: tudo que afeta a documentação vai no relatório da tarefa.
- **Cristopher**: aprova planos, testa no editor e decide.

### Canal entre chat e Code: banco Tarefas do Notion

Cada tarefa é uma página do banco Tarefas. O corpo da página tem três seções, cada uma com um único autor:

1. **Briefing** (chat): objetivo; decisões que se aplicam (números); escopo e fora do escopo; pronto quando (critério verificável no editor); arquivos prováveis (Reverse-lookup).
2. **Relatório** (Code): resultado; arquivos criados ou alterados, com uma linha de propósito cada; desvios do briefing e por quê; o que configurar no editor; resultado do build; hash do commit.
3. **Para o chat** (Code): decisões em aberto e propostas; divergências entre código e documentação; o que mudar no Notion (Inventário de Arquivos, Reverse-lookup, Gameplay Tags, Contratos de Autoridade, Pitfalls Críticos, Valores Provisórios) e neste Context Pack.

Status:

- **A fazer**: briefing pronto (chat).
- **Fazendo**: o Code começou.
- **Aguardando teste**: o Code terminou e compilou; o Cristopher testa no editor.
- **Aguardando chat**: o Code parou numa decisão em aberto, ou a tarefa foi confirmada e commitada e a seção "Para o chat" espera o chat.
- **Feito**: o chat aplicou a documentação.

Tarefas do tipo **Análise** não mudam o repositório: o Code lê o código e responde no Relatório. Servem para o chat decidir com base no código atual, que ele não vê.

**Tarefa expressa**: uma correção trivial pode ir direto ao Code, sem briefing. O Code cria a página (tipo Bug), preenche Relatório e "Para o chat" e segue o mesmo fluxo de status.

### Regras para os dois

- Arquitetura primeiro; soluções padrão da indústria; migrações completas, sem caminho legado.
- Perguntar quando faltar informação, com perguntas focadas e de múltipla escolha.
- Sinalizar contradições e decisões em aberto; manter decisões separadas de números provisórios.
- Sem narrar o processo de investigação: diagnóstico resumido, mudanças e explicação breve só do que for mais complexo do que o trivial.
- Quando um design depender de detalhes do código atual, abrir antes uma tarefa de Análise.

---

## 8 — Notion (fonte canônica)

Página **Nevergone**, dentro de Game Dev:
https://app.notion.com/p/3de6b268999881a29b02df5cb9f48c34

- Decisões — https://app.notion.com/p/3de6b268999881d9a4c7fad9459bc91b
- Valores Provisórios — https://app.notion.com/p/3de6b268999881f7bfcadb31745baee3
- Recomendações e Perguntas Abertas — https://app.notion.com/p/3de6b26899988106b7ecdfbe77ba655e
- Escopo e Reaproveitamento — https://app.notion.com/p/3de6b268999881c5b7d6c04dbd132171
- Milestones — https://app.notion.com/p/3de6b268999881f08c08c7a2f63c705a
- Playtest Notes — https://app.notion.com/p/3de6b2689998810e851bed7aa5aa66b9
- Tarefas (banco) — https://app.notion.com/p/239f79615b964b0b8746433839c3bd60
- Referência Técnica — https://app.notion.com/p/3de6b2689998810faf7fda20e23f5e13
  - Arquitetura, Contratos de Autoridade, Layout do Módulo e Convenções, Pitfalls Críticos, Gameplay Tags, Inventário de Arquivos, Reverse-lookup

**Perguntas em aberto** (ver a página correspondente): nome e tema do jogo; zumbis ou outra criatura; recrutamento de sobreviventes além do resgate na excursão; lista final de atividades diurnas; tipos de inimigo iniciais; função exata do veículo; número de tiers da pesquisa; geração do mapa de viagem; o que acontece com a cidade esgotada.
