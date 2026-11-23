# Plan d'optimisation de l'émulateur CPU6 en C++26

## Contexte et mesures de référence

Sur AMD Ryzen 9 5950X avec `g++-15` (`-O3 -flto`), les performances actuelles sont :

| Scénario | Commande | Débit actuel |
|---|---|---|
| Cœur CPU6 seul (JS vs C++) | `python3 tests/compare-emulator-cores.py --rounds 1 --cycles 50000000` | JS **16,13 M** microcycles/s, C++ **46,84 M** microcycles/s (**2,90×**, ~107 cycles hôtes/microcycle) |
| Machine complète (boot Hawk + CENTOS) | `time ./build/out/emulator-cpp/cpp-hawk-boot build/out/emulator/firmware.json swext/centurion-js/src/firmware-spec.json build/out/image/centos-apps.img` | **25,9 M** microcycles/s (27,1 M microsteps en 1,044 s) |

### Causes identifiées (vérifiées dans le code et le désassemblage `objdump`)

1. **Mur de prédiction de branchement dans `Cpu6::step()` (`emulator/cpu6-step.cpp`)**
   - Portage littéral de `hsstep()` (`swext/centurion-js/src/cen.ts`).
   - Le binaire émet **8 sauts indirects par table de saut (`notrack jmp *%rax`)** plus ~10 branches conditionnelles imprévisibles à chaque microcycle (`dp_sel`, `aluc_s`, `fn.operation`, `x_k11`, `x_e7`, `x_h11`, `x_e6`, `aluc_d`, `k9_sel`, `j13_sel`, `k13_sel`, et les 3 séquenceurs).
   - `Cpu6::step()` fait **8,6 Ko** et n'est pas inlinée dans `Cpu6::run()` : chaque microcycle subit `call`/`ret`, un prologue poussant 6 registres callee-saved + 184 octets de pile (`sub $0xb8, %rsp`), et des rechargements mémoire avec offsets 32 bits (`+0x14b47(%rdi)`) car `microcode_` (70 Ko) et `microcode_words_` (14 Ko) sont placés avant les champs chauds dans `Cpu6`.

2. **Calcul de parité RAM coûteux (`emulator/backplane.hpp`, `emulator/Makefile`)**
   - Sans `-march=native` / `-mpopcnt`, `std::popcount(value)` dans `Backplane::read()` / `Backplane::write()` génère un **appel PLT à `__popcountdi2@plt`** à chaque accès RAM.
   - De plus, `parity_[addr] != odd_parity(ram_[addr])` est mathématiquement identique au stockage d'un simple indicateur `bad_parity` (qui vaut presque toujours `false`).

3. **Surcharge des périphériques `fast_devices_` dans `Machine` (`emulator/finch.cpp`, `emulator/cmd.cpp`, `emulator/backplane.cpp`)**
   - `bus_.tick_fast()` effectue deux appels virtuels à **chaque** microstep CPU6 (`Finch::tick(5)` et `Cmd::tick(5)`), ce qui réduit presque de moitié le débit de `Machine` (46,8 M/s → 25,9 M/s) même lorsqu'aucun disque Finch n'est monté et que `Cmd` est à l'état `Idle`.

4. **Redondances dans le cœur CPU6 (`emulator/cpu6.hpp`, `emulator/cpu6.cpp`, `emulator/cpu6-step.cpp`)**
   - 3 objets `Sequencer` 4 bits distincts (`s0`, `s1`, `s2`) alors qu'ils partagent toujours `seq_fc` et `sp`, et que **1708 / 2048** microinstructions utilisent le même sélecteur pour les 3 tranches.
   - `tick_timer()` évalue plusieurs branches à chaque microstep au lieu d'un simple décompteur.
   - `Backplane::interrupt_pending()` boucle sur `interrupt_devices_` avec appels virtuels `device->interrupt_level()`.

---

## Objectifs et contraintes

- **Passer en `-std=c++26`** dans `emulator/Makefile` (vérifié : `g++-15 -std=c++26 -O3 -Wall -Wextra -Wpedantic` supporte `#embed`, `__has_embed`, `--embed-dir=...` et `[[gnu::musttail]]` sans aucun warning).
- **Préserver l'exactitude bit-à-bit au microcycle près** :
  - `tests/compare-emulator-cores.py` (trace de 10 000 microcycles + hash CPU/RAM sur 50–100 M microcycles) doit passer à l'identique.
  - `make tests-emulator-cpp` (y compris la comparaison binaire `cmp native.ffi reference.ffi` de Finch, CMD, diagnostic ROM, telnet/kermit, web-console et `apps-image.js --compare`) doit passer à 100 %.
- **Ne jamais commiter de ROMs dans le dépôt** : l'incorporation de `firmware.json` via `#embed` doit être protégée par `#if __has_embed("firmware.json")` avec repli automatique sur l'interpréteur générique si le fichier est absent au build ou si une autre ROM est chargée au runtime.

---

## Étapes d'implémentation recommandées

Implémenter et benchmarker étape par étape pour mesurer le gain de chaque chantier.

### Étape 1 — Migration C++26 & optimisations `Backplane` / `Makefile`

1. **`emulator/Makefile`** :
   - Remplacer `-std=c++23` par `-std=c++26`.
   - Ajouter `-march=native` (ou une variable `ARCHFLAGS ?= -march=native` surchargeable pour les builds portables).
   - Ajouter `--embed-dir=$(abspath $(dir $(FIRMWARE_JSON)))` (avec `FIRMWARE_JSON ?= $(ROOT)/build/out/emulator/firmware.json`, seulement si le répertoire existe ou sur `$(ROOT)/build/out/emulator`). Note importante vérifiée sur `g++-15` : `-I` ne suffit **pas** pour `#embed`, il faut impérativement `--embed-dir=...`.

2. **Éliminer `std::popcount` dans `emulator/backplane.hpp` et `emulator/backplane.cpp`** :
   - Remplacer `std::array<std::uint8_t, io_base> parity_ {};` par :
     - `std::array<std::uint8_t, io_base> bad_parity_ {};` (0 = parité valide, 1 = parité inversée).
     - `std::uint32_t bad_parity_count_ {};` (ou `bool has_bad_parity_ {}`).
   - Dans `load_ram(base, bytes)` : si `has_bad_parity_`, remettre à 0 `bad_parity_[base .. base + bytes.size()]` (sinon rien à faire).
   - Dans `write(address, value, invert_parity)` :
     - Si `!invert_parity && !has_bad_parity_` (chemin rapide 99,99 % du temps) : ne même pas toucher `bad_parity_` !
     - Sinon : écrire `bad_parity_[address] = invert_parity` et mettre à jour `has_bad_parity_` (ou `bad_parity_count_`).
   - Dans `read(address, parity_fault)` :
     - `parity_fault = has_bad_parity_ ? (bad_parity_[address] != 0) : false;`
   - Résultat : zéro appel à `popcount`, et empreinte cache RAM divisée par deux (252 Ko au lieu de 504 Ko).

3. **Cache du niveau d'interruption sur `Backplane`** :
   - Ajouter un mécanisme permettant aux périphériques IRQ (actuellement `Mux`) de notifier `Backplane` quand leur niveau d'IRQ change, ou exposer `max_irq_level_` sur `Backplane` afin que `bus_.interrupt_pending(level)` soit une simple comparaison entière `max_irq_level_ > level` sans boucle virtuelle.
   - Si on garde l'API `Device::interrupt_level()` générique, vérifier que `Mux` appelle une méthode `bus.update_interrupts()` (ou `bus.set_irq_level(...)`) aux 4 endroits où `enabled_`, `pending_` ou `level_` changent (`reset`, `acknowledge`, `interrupt`, `write` cases 10/13/14).

### Étape 2 — Optimisations du cœur `Cpu6` (layout, séquenceur 12 bits, timer)

1. **Réorganiser le layout mémoire de `Cpu6` (`emulator/cpu6.hpp`)** :
   - Placer **tous les champs scalaires chauds** (`micro_pc_`, `workaddr`, `memaddr`, `physaddr`, `alu_flag`, `ccr_*`, `busctl`, `sysctl`, `result`, `rir`, `rdr`, `swap`, `memdata_in`, `memdata_out`, `pta`, `pindex`, `pgaddr`, `is_mmio`, `is_regmmio`, `notpgbit`, `is_mem`, `interrupt_level_`, `reqlevel`, `b15a`, `dma_current`, `dma_12`, séquenceur, `microsteps_`, `timer_countdown_`) au **tout début** de `Cpu6` (dans les 64–128 premiers octets, pour bénéficier des offsets `disp8` et de la localité L1).
   - Déplacer les gros tableaux (`microcode_`, `microcode_words_`, `map_rom_`, `regfile`, `pagetb`, `alu_registers_`) après les champs chauds.
   - Compacter `MicroInstruction` (ex. : champs bitfield ou types compacts) et y pré-calculer :
     - `inv_r_mask` (`0x00` ou `0xff`) et `inv_s_mask` (`0x00` ou `0xff`),
     - `alu_op` (0=Add, 1=Or, 2=And, 3=Xor),
     - `seq_d` sur 12 bits : `(seq_d2 << 8) | (seq_d1 << 4) | seq_d0`.

2. **Unifier les 3 séquenceurs 4 bits (`s0, s1, s2`) en un seul séquenceur 12 bits (`emulator/cpu6.hpp`, `emulator/cpu6.cpp`, `emulator/cpu6-step.cpp`)** :
   - Remplacer `s0, s1, s2` par un seul état :
     - `std::uint16_t seq_p {}`, `seq_h {}` (noter que les bits 8..11 de `seq_h` sont toujours 0, seuls `s0.h` et `s1.h` sont chargés par `x_e6 == 6`),
     - `std::array<std::uint16_t, 4> seq_stack {}`,
     - `std::uint8_t seq_sp {}`.
   - Attention à la sémantique exacte du carry entre tranches 4 bits :
     - Quand `seq0_s == seq1_s == seq2_s` (83 % des microinstructions, et toujours vrai quand `jump_to_subroutine != 0`), les 3 tranches choisissent la même source 12 bits `src` (`seq_p`, `seq_h`, `seq_stack[seq_sp]` ou `mci.seq_d`), puis :
       ```cpp
       const std::uint16_t output = (src | seq_or) & 0xfff;
       const std::uint16_t next = (output + 1) & 0xfff;
       ```
       (vérifier : `s0.resolve(true, ...)` ajoute `+1` au nibble bas, propage le carry vers `s1` si `out0 == 15`, puis vers `s2` si `out1 == 15` et `c`, ce qui est strictement identique à `(output + 1) & 0xfff` !).
     - Quand les 3 sélecteurs diffèrent (17 % des cas), on compose simplement les 3 nibbles en `output = ((n2 << 8) | (n1 << 4) | n0) | seq_or`, puis `next = (output + 1) & 0xfff` — **le `+ 1` sur 12 bits reste strictement identique** !
   - `commit(seq_fc)` se fait en une seule opération sur `seq_sp`, `seq_stack`, `seq_p`.
   - Mettre à jour `micro_state()` pour reconstruire le même `MicroState` qu'auparavant.

3. **Décompteur pour `tick_timer()` (`emulator/cpu6.cpp`)** :
   - Dans le cas courant (`!boot_guard_ || memaddr >= 0x0600`), remplacer la logique de `tick_timer()` par un décompteur `timer_countdown_` qui ne déclenche l'appel lent `handle_timer_edge()` qu'une fois tous les 2 000 cycles (en `HostClock`) ou 83 333 cycles (en `Emulated`).
   - Attention à conserver strictement le comportement de `boot_guard_` (`memaddr < 0x0600` remet `cycles_ = 0` et `rtc = false`) et de `!(sysctl & 0x20)` (`rtc = false` — à maintenir lors des écritures dans `sysctl` et à l'expiration du timer).

### Étape 3 — Compilation AOT du microcode en C++26 (`#embed` + `consteval` + `[[gnu::musttail]]`)

C'est le cœur du gain (objectif : supprimer les 8 sauts indirects internes et les 12 `switch` par microstep).

1. **Extraction `consteval` depuis `firmware.json` (`emulator/cpu6-compiled.cpp` ou header interne `emulator/cpu6-microcode-rom.hpp`)** :
   ```cpp
   #if __has_embed("firmware.json")
   #define CENTURION_HAS_EMBEDDED_MICROCODE 1
   namespace {
   constexpr char embedded_firmware_json[] = {
   #embed "firmware.json"
   };
   }
   #endif
   ```
   - Écrire un parseur `consteval` minimaliste (~60 lignes) qui :
     - Localise dans `embedded_firmware_json` les 7 clés JSON `"e"`, `"j"`, `"m"`, `"l"`, `"h"`, `"f"`, `"k"` (et optionnellement `"map"` pour la MAPROM de 256 octets) dans l'objet `"roms"`.
     - Décode leurs chaînes base64 en `std::array<std::uint8_t, 2048>` (et 256 pour `"map"`).
     - Calcule `constexpr std::array<MicroInstruction, 2048> k_rom` avec exactement les mêmes équations que `Cpu6::load_microcode()` (`emulator/cpu6.cpp:31-72`) + un hash FNV-1a 64 bits `k_rom_hash` calculé sur les 7×2048 octets bruts (plus les 256 octets de `map` si `map` est aussi embarquée en `constexpr`).
   - Dans `Cpu6::load_microcode()` / constructeur de `Cpu6` : calculer le même hash FNV-64 sur la ROM chargée dynamiquement ; poser `use_compiled_core_ = (loaded_hash == k_rom_hash)`.
   - Dans `Cpu6::run(cycles)` : si `use_compiled_core_ && cycles > 0`, dispatcher vers `run_compiled(cycles)`, sinon conserver la boucle de repli `step()`.

2. **Spécialisation par adresse `template <std::uint16_t Addr>` + `[[gnu::musttail]]`** :
   - Définir une signature de handler non-`noexcept` (puisque `Backplane`/`DmaDevice` ne sont pas `noexcept`) et uniforme pour garantir les tail-calls :
     ```cpp
     using StepHandler = void (*)(Cpu6& cpu, std::uint64_t remaining);
     ```
   - Déclarer la table `extern const std::array<StepHandler, 2048> k_handlers;` (ou `static constexpr`).
   - Dans `template <std::uint16_t Addr> void exec_micro(Cpu6& cpu, std::uint64_t remaining)` :
     - `constexpr auto mci = k_rom[Addr];`
     - Exécuter la logique de `Cpu6::step()` en utilisant `if constexpr` sur tous les champs de `mci` :
       - `if constexpr (Addr == 0x101) cpu.last_pc_ = cpu.memaddr;`
       - `if constexpr (mci.k9_en)` -> une seule branche exacte pour `mci.k9_sel`.
       - `dp_sel`, `x_f6h6`, `aluc_s`, `alu_op`, `inv_r_mask`, `inv_s_mask`, `x_h11 == 6`, `aluc_d == 2`, `aluc_d > 3`, `seq_branch == 0` (`j13_sel`, `k13_sel`), `x_c14_sel`, `rir0`, `x_k11`, `x_e7`, `x_h11`, `x_e6` (`j12_sel`, `j11_en`/`j11_sel`, `j10_en`/`j10_sel`), `aluc_d` : **tous résolus à la compilation**.
       - Mettre à jour `cpu.micro_pc_ = next_pc; ++cpu.microsteps_;` et appeler `cpu.bus_.tick_fast()` seulement si `cpu.bus_.has_active_fast_devices()`.
       - Décrémenter :
         ```cpp
         if (--remaining == 0)
             return;
         ```
     - **Enchaînement par tail-call spécialisé (point crucial pour la perf)** :
       - Analyser en `consteval` le type de transition de `Addr` :
         1. **Transition statique directe** (`!mci.k9_en && mci.seq_branch == 1 && mci.seq0_si == 3 && mci.seq1_si_k6dc == 3 && mci.seq2_si_k6b == 3`) :
            `constexpr std::uint16_t target = mci.seq_d & 0x7ff;`
            `[[gnu::musttail]] return exec_micro<target>(cpu, remaining);` → **saut direct `jmp`, zéro indirection !**
         2. **Transition conditionnelle `k9_en` uniquement** (`mci.k9_en && mci.seq_branch == 1`) :
            - Si `jump_to_subroutine` est vrai → la cible est **toujours statique** (`mci.seq_d & 0x7ff`) : `[[gnu::musttail]] return exec_micro<jsr_target>(cpu, remaining);`
            - Si `jump_to_subroutine` est faux et que le chemin normal est aussi statique → `[[gnu::musttail]] return exec_micro<normal_target>(cpu, remaining);` (2 sauts directs, zéro table de dispatch !).
         3. **Autre transition dynamique** (séquenceur sur `seq_p`, `seq_h`, `seq_stack[seq_sp]`, ou `seq_or` variable) :
            `[[gnu::musttail]] return k_handlers[next_pc](cpu, remaining);`
            *(Note : si `mci.seq_branch == 0` avec base `seq_d`, on peut même vérifier si seulement 2 ou 4 cibles sont possibles et utiliser des `if` + `[[gnu::musttail]]` directs si cela améliore encore le benchmark).*

3. **Maîtriser le temps de compilation** :
   - Instancier 2048 spécialisations dans une seule TU sous `-O3 -flto` peut être lourd en temps et en RAM.
   - Recommandation : découper l'instanciation en **8 fichiers `.cpp` de 256 adresses** (ou un seul macro-générateur compilé avec `-DBLOCK_INDEX=0..7`, par ex. `cpu6-compiled-0.cpp` .. `cpu6-compiled-7.cpp` ou règle Makefile générique), déclarant `exec_micro<Addr>` en `extern template` / table partagée. Cela permet la compilation parallèle (`make -j`) et garde un temps de build de quelques secondes.
   - Si `-flto` sur ces 8 objets ralentit trop le lien sans gain mesurable, envisager de compiler ces tranches avec `-fno-lto` (les sauts `[[gnu::musttail]]` entre tranches restent des `jmp` directs résolus par le linker !).
   - **Bonus optionnel (si le gain n'atteint pas encore l'objectif)** :
     - Vérifier `objdump` sur quelques `exec_micro<Addr>` chauds : si GCC recharge trop de champs entre microinstructions consécutives, on peut soit :
       - inliner en `constexpr` jusqu'à 2–4 microsteps linéaires (transition statique sans effet de bord externe) avant de faire le `musttail` (attention à décrémenter `remaining` exactement et à gérer `remaining < block_len` en retombant sur le handler 1-step),
       - ou spécialiser `exec_micro` en deux variantes (ou garder `has_active_fast_devices()` hors de la boucle chaude).

### Étape 4 — Éliminer la perte de 45 % sur `Machine` (`Finch` et `Cmd` au repos)

Aujourd'hui, `cpp-hawk-boot` tourne à 25,9 M/s contre 46,8 M/s sur `cpu6-probe` uniquement à cause de `bus_.tick_fast()`.

1. **Masque d'activité dans `Backplane` (`emulator/backplane.hpp`, `emulator/backplane.cpp`)** :
   - Associer un bit d'activité à chaque périphérique rapide (`std::uint32_t fast_active_mask_ {}`), ou exposer `[[nodiscard]] bool has_active_fast_devices() const noexcept { return fast_active_mask_ != 0; }`.
   - Dans `Cpu6` : ne même pas appeler `bus_.tick_fast()` si `!bus_.has_active_fast_devices()` (1 seule branche prédite non-prise).

2. **`Cmd` (`emulator/cmd.hpp`, `emulator/cmd.cpp`)** :
   - `Cmd::tick()` n'a d'effet observable que lorsque `phase_ == Phase::Delay || input_pending_` (voir `emulator/cmd.cpp:91-102`).
   - Activer le bit rapide de `Cmd` uniquement quand `phase_ == Phase::Delay || input_pending_`, et le désactiver dès que `Cmd` revient à un autre état (`Idle`, `Transfer`, `Finish`, etc.).
   - Vérifier que `tests/cpp-peripherals-probe.cpp` passe toujours.

3. **`Finch` (`emulator/finch.hpp`, `emulator/finch.cpp`)** :
   - **Attention** : `tests-emulator-cpp` compare l'image disque Finch octet par octet avec le simulateur JS (`cmp native.ffi reference.ffi`). Il ne faut **rien** altérer quand une image Finch est montée ou que la carte est sollicitée.
   - Quand `!image_` (aucun disque Finch monté), après `reset()`, le contrôleur Finch exécute son initialisation microcode puis boucle indéfiniment en attente de `from_host_` sans jamais modifier aucun bit visible par l'hôte (`busy_`, `status_2_`, `from_host_`, `to_host_`, `dma_active_`, `dma_cycle_`).
   - Stratégie sûre :
     - Après `reset()`, laisser `Finch` tourner jusqu'à détecter qu'il est dans sa boucle d'attente passive avec `!image_ && selected_unit_ != 1 && !from_host_ && !dma_active_ && !dma_cycle_` (par ex. en vérifiant qu'un tour complet de boucle d'attente n'a modifié ni `busy_`, ni `status_2_`, ni `to_host_`, ni `ram_`, ou plus simplement : détecter la boucle d'attente stable du firmware Finch quand `!image_`).
     - Une fois en veille (`quiescent_ = true`), désactiver le bit rapide de `Finch` dans `Backplane`.
     - Réveiller immédiatement `Finch` (`quiescent_ = false` + réactivation du bit rapide) dans `Finch::reset()`, `Finch::mount()`, `Finch::write()`, et si besoin `Finch::sample_signals()`.
   - Vérifier que `cpp-finch-probe` + `cmp native.ffi reference.ffi`, `cpp-hawk-boot`, et `tests-emulator-cpp` passent tous à 100 %.

---

## Protocole de validation et benchmarks

Après chaque étape, lancer :

1. **Compilation propre (zéro warning)** :
   ```sh
   make emulator-cpp
   ```
2. **Exactitude microcycle-à-microcycle + benchmark cœur CPU6 vs JS** :
   ```sh
   python3 tests/compare-emulator-cores.py --rounds 3 --cycles 100000000
   ```
3. **Benchmark machine complète (Hawk + CENTOS)** :
   ```sh
   time ./build/out/emulator-cpp/cpp-hawk-boot \
       build/out/emulator/firmware.json \
       swext/centurion-js/src/firmware-spec.json \
       build/out/image/centos-apps.img
   ```
4. **Suite de régression complète C++ (Finch bit-à-bit, CMD, diag ROM, Telnet/Kermit, Web console, Apps)** :
   ```sh
   make tests-emulator-cpp
   ```
5. **Test du repli sans `firmware.json`** :
   Vérifier qu'en compilant sans `--embed-dir` valide (ou avec un répertoire vide), `#if __has_embed("firmware.json")` bascule proprement sur l'interpréteur générique sans erreur ni warning.
