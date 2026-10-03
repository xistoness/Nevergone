# Convenções de código — Nevergone

Carregado pelo Claude Code em toda sessão. Muda só quando um Briefing pedir.

## Estrutura

- Unreal Engine 5.7, C++ nativo + GAS. Nada de lógica em Blueprint: Blueprints só guardam assets e valores.
- Módulo único `Nevergone`, com `Public/` e `Private/` espelhados.
- Includes escritos a partir da raiz do módulo (`#include "ActorComponents/X.h"`), nunca `Public/...`.

## Estilo

- Comentários em inglês.
- Código novo loga com `UE_LOG(LogNevergone, ...)`, prefixo `[NomeDaClasse]` e o sujeito via `*GetNameSafe(...)`. Arquivos antigos que usam `LogTemp` mantêm o padrão do arquivo.
- `TObjectPtr` para referências possuídas, `TWeakObjectPtr` para caches entre sistemas, `TSoftObjectPtr`/`TSoftClassPtr` para o que é referenciado por save data ou por variante de arte.
- Preserve o fim de linha de cada arquivo: há arquivos CRLF e LF, e alguns com BOM UTF-8. Arquivo novo: LF, sem BOM.
- Migração completa: ao trocar uma API, atualize todos os chamadores e remova o caminho antigo no mesmo trabalho.
- Soluções padrão da indústria e do engine. Quando o padrão não estiver claro, pergunte antes de propor algo próprio.
- Nada fora do escopo do Briefing: refatorações e melhorias que você notar vão para "Para o chat".

## Regras do engine e do projeto

1. **`OnPostRestore` roda antes de `BeginPlay`** no carregamento de mundo. Inicialização de estado saveable vai em `OnPostRestore`; `BeginPlay` só consome.
2. **Gameplay tags nativas precisam do par:** `UE_DECLARE_GAMEPLAY_TAG_EXTERN` no header e `UE_DEFINE_GAMEPLAY_TAG` no .cpp, ambos em `Nevergone.h/.cpp`. Nunca `RequestGameplayTag` em runtime.
3. **`SwapPlayerControllers` não destrói o controller antigo:** `Destroy()` explícito.
4. **`UPROPERTY` de GameInstanceSubsystem não é editável em Class Defaults.** Autorar em `UMyGameInstance` e espelhar no `Init()`. Constantes ajustáveis sem recompilar vão num `UDeveloperSettings`.
5. **Autoridade de dados:** durante combate, ler HP/AP/team/status de `FBattleUnitState` via `UBattleState::FindUnitState`; fora de combate, de `FPartyMemberData` via `FindMemberMutable` + `NotifyMemberMutated`. Nunca escrever direto em `UMyGameInstance::PartyData`. Exceção: atributos base (Força etc.) não existem em `FPartyMemberData`; ler do `UUnitStatsComponent` do pawn (`Attributes` + `TemporaryAttributeBonuses`).
6. **Configuração de habilidade vive no `UAbilityDefinition`**, não na classe GA (template sem estado). Cooldowns são por definição, em `UBattleModeComponent::DefinitionCooldowns`.
7. **GUID após destroy-and-respawn:** `USaveableComponent::SetSaveIdentifier(OldId)`.
8. **Overlaps com respawn no mesmo lugar** precisam de gate de saída-antes-de-retrigger.
9. **`CaptureSnapshot` idempotente** em `UCombatPresenceComponent`.
10. **`FText::FromString` não localiza.** Usar `LOCTEXT`/`NSLOCTEXT`/`FText::Format`, FText em DataAssets ou metadata de UEnum. Separadores de layout: `INVTEXT`.
11. **Shield absorve antes de mutar HP** em `NotifyDamageApplied`.
12. **Imunidade:** `Immunity.<StateTag com pontos como underscores>`.
13. **Aparência sorteada precisa ser determinística.** Variante visual de inimigo deriva do `FName` do ator, nunca de `FMath::RandRange` em runtime. Ao spawnar em runtime (P3), a semente migra para valor persistido via `USaveableComponent`.
14. **Input da exploração pertence ao controller.** `AExplorationPlayerController` é dono do mapping context e das Input Actions; tudo chega ao sobrevivente pela `IExplorationInputReceiver` → `UExplorationModeComponent`. `ANevergoneCharacter` não tem input.
15. **Telas pausam, captura não.** No `AExplorationPlayerController`, todo modo de tela pausa o mundo (`SetPause`); a captura congela os outros atores por `CustomTimeDilation`, porque o mundo pausado silencia os sons 3D. Input Actions usadas em telas precisam de "Trigger When Paused".
16. **Quem os inimigos caçam** é decidido só por `AExcursionSurvivorNPC::IsSurvivorBody`. Percepção, props e captura perguntam ali.
