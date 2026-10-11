# ⚔️ AikaHUD - Macro de Combate & Overlay Nativo em C Puro (Win32)

Overlay gamer translúcido e assistente autônomo de combate para o **Aika Online**, escrito inteiramente em **C99 nativo (Win32 API / GDI puro)**, com arquitetura de injeção de comandos de baixa latência e bypass de hooks do GameGuard baseada na tecnologia validada do `aion2-bot`.

---

## 🚀 Destaques e Arquitetura

- **Binário Ultra-Leve:** Apenas **~44 KB** (com manifesto UAC de Administrador e ícone oficial multi-resolução embutidos). Sem Python, sem .NET, sem Electron e sem dependências externas.
- **Consumo Mínimo de Recursos:** **0.0% de CPU** e **~1.8 MB de RAM**, rodando com suavidade total.
- **Interface Obsidian Glassmorphism:** Renderizada com double-buffering via Win32 GDI (zero flicker, zero tearing), arrastável e sempre no topo (`WS_EX_TOPMOST`).
- **Detecção Instantânea de Alvo por Memória GDI (`< 0.5 ms`):** Amostragem direta via `BitBlt` da barra carmesim de vida do alvo em `1080p` (`Y = 48, X = 860..1036`). O bot sabe exatamente se há um monstro selecionado e quando ele é derrotado.
- **Ciclo Completo com Auto-Loot:** 
  1. **Modo Busca:** Dispara `TAB` para focar o monstro mais próximo.
  2. **Modo Combate:** Aciona o **Ataque Normal (Slot 5)** para o personagem correr até o mob e manter auto-ataque, intercalando com a rotação de habilidades (`1`, `2`, `3`) e cura (`4`).
  3. **Morte do Monstro & Coleta:** Quando a barra de vida do alvo desaparece, contabiliza o mob derrotado e dispara `SPACE` (Auto-Loot) antes de procurar o próximo.
- **Bypass de Hooks do GameGuard:** Em vez de usar `keybd_event` (que aciona flags de injeção interceptadas pelo hook `WH_KEYBOARD_LL`), o motor utiliza envio dual:
  - `PostMessageW(hwnd, WM_KEYDOWN / WM_KEYUP)` direto na fila de mensagens do `AikaClient Window`.
  - `SendInput` com flag estrita `KEYEVENTF_SCANCODE` e mapeamento dinâmico de scancode de hardware (`MapVirtualKeyW`).
- **Totalmente Customizável (`config.ini`):** Permite alterar teclas, slots e tempos de espera sem recompilar o programa.

---

## ⚠️ Configuração Obrigatória no Jogo (Slot 5)

No Aika Online, usar uma habilidade especial de longe não faz o personagem andar até o monstro. O comando que faz o personagem **correr até o alvo e iniciar o combate corpo-a-corpo** é o **Ataque Normal**:

1. Abra a janela de Habilidades no jogo pressionando **`K`**.
2. Localize o ícone de **"Ataque Normal"** (duas espadas cruzadas).
3. Arraste-o para o **Slot 5** da sua barra de ação principal.
4. Mantenha suas habilidades de ataque nos slots **1**, **2** e **3**, e sua poção/cura no slot **4**.

---

## 🎮 Controles e Atalhos Globais

| Tecla / Atalho | Função | Descrição |
| :--- | :--- | :--- |
| **`F8`** | **Ligar / Pausar Macro** | Alterna o ciclo de combate automático. Pode ser clicado no HUD. |
| **`F9`** | **Auto-Target (TAB)** | Liga/desliga a busca automática de monstros com `TAB`. |
| **`F10`** | **Auto-Loot (SPACE)** | Liga/desliga a coleta automática de drops após a morte do mob. |
| **`F7`** | **Auto-Cura (Slot 4)** | Liga/desliga o uso periódico de poção ou habilidade de suporte. |
| **`F12`** | **Encerrar HUD** | Fecha o aplicativo e limpa todos os atalhos. |

---

## ⚙️ Arquivo de Configuração (`config.ini`)

O arquivo `config.ini` é gerado automaticamente na mesma pasta do executável:

```ini
[Combate]
; Tecla de Ataque Basico / Correr ate o monstro (coloque "Ataque Normal" neste slot no jogo)
KeyBasic=5

; Teclas de Habilidades principais na barra de acao
KeySkill1=1
KeySkill2=2
KeySkill3=3

; Tecla de Pocao ou Cura
KeyHeal=4

; Tecla de Coleta de Itens (Loot) - padrao SPACE
KeyLoot=SPACE

; Intervalo entre habilidades em milissegundos
SkillRotationMs=1000

; Intervalo de reforco de ataque normal / aproximacao (ms)
BasicAttackMs=600

; Intervalo de cura / pocao (ms)
HealCooldownMs=6000

; Quantidade de vezes que pressiona loot ao derrotar monstro
LootPressCount=2

[Modulos]
AutoTarget=1
AutoLoot=1
AutoHeal=1
```

---

## 🛠️ Como Compilar

O projeto utiliza o **Tiny C Compiler (TCC)** para compilação instantânea sem dependências pesadas do Visual Studio:

1. Dê dois cliques em `build.bat` ou execute pelo terminal:
   ```cmd
   cmd /c build.bat
   ```
2. O script irá:
   - Compilar `src/main.c`, `src/combat.c` e `src/ui.c` em `AikaHUD.exe`.
   - Injetar o manifesto UAC de Administrador (`tools_inject_manifest.py`).
   - Injetar o ícone de alta resolução nos recursos do PE (`tools_inject_icon.py`).
   - Copiar automaticamente o `AikaHUD.exe` e o `config.ini` atualizados para a sua **Área de Trabalho**.

---

## 📂 Estrutura do Projeto

```
aika-hud/
├── src/
│   ├── main.c              # Ponto de entrada Win32, UAC elevation, WndProc e timer
│   ├── combat.c            # Máquina de estados, bypass GameGuard, BitBlt e config.ini
│   ├── combat.h            # Estruturas de estado, configuração e assinaturas
│   ├── ui.c                # Renderização Obsidian Glassmorphism em GDI e handlers
│   └── ui.h                # Constantes dimensionais e protótipos de interface
├── build.bat               # Script automatizado de compilação e deploy
├── config.ini              # Arquivo de configuração de teclas e tempos
├── tools_inject_manifest.py# Injetor PE de manifesto requireAdministrator
├── tools_inject_icon.py    # Injetor PE de ícone multi-formato
├── aikahud.ico             # Ícone do projeto
└── README.md               # Documentação técnica completa
```
