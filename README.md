# Simulador do Algoritmo de Tomasulo

Simulador do Algoritmo de Tomasulo implementado em C - Disciplina de Arquitetura de Computadores 3. O programa lê um arquivo `.txt` com instruções assembly MIPS simplificado e executa o pipeline superescalar ciclo a ciclo, exibindo o estado completo das estações de reserva, registradores e instruções a cada etapa.

---

## Sumário

- [Compilação e Execução](#compilação-e-execução)
- [Formato do Arquivo de Entrada](#formato-do-arquivo-de-entrada)
- [Saída do Programa](#saída-do-programa)
- [Estrutura do Código](#estrutura-do-código)
- [Detalhes de Implementação](#detalhes-de-implementação)
- [Casos de Teste](#casos-de-teste)
- [Referências](#referências)

---

## Compilação e Execução

### Compilar

```bash
gcc -Wall -Wextra -std=c11 superescalar.c -o superescalar
```

### Executar

```bash
./superescalar <arquivo_de_instrucoes.txt>
```

Se nenhum argumento for passado, o simulador tenta ler o arquivo padrão `./programa/test.txt`.

**Exemplos de execução com os arquivos de teste incluídos:**

```bash
./superescalar ./programa/test_dependencias.txt
./superescalar ./programa/test_renomeacao.txt
./superescalar ./programa/test_estacoes_cheias.txt
./superescalar ./programa/test_memoria.txt
```

---

## Formato do Arquivo de Entrada

### Instruções aritméticas / lógicas

```
op  rd, rs, rt
```

Exemplo:

```
add t1, t2, t3
sub t4, t1, t0
mul t5, t1, t2
div t6, t5, t3
or  t1, t2, t3
and t7, t1, t4
```

### Instruções de memória

```
lw  rd, offset(base)
sw  rd, offset(base)
```

Exemplo:

```
lw  t0, 0(t1)
sw  t2, 4(t3)
```

### Registradores disponíveis

O simulador define **10 registradores** nomeados `t0` a `t9`. Cada um é inicializado com o valor `i * 10` (ou seja, `t0 = 0`, `t1 = 10`, `t2 = 20`, ..., `t9 = 90`).

### Instruções suportadas

| Opcode | Operação             | Unidade Funcional | Ciclos de EX |
|--------|----------------------|-------------------|--------------|
| `add`  | Adição inteira       | Add (RS_ADD)      | 1            |
| `sub`  | Subtração inteira    | Add (RS_ADD)      | 1            |
| `or`   | OR bit a bit         | Add (RS_ADD)      | 1            |
| `and`  | AND bit a bit        | Add (RS_ADD)      | 1            |
| `mul`  | Multiplicação        | Mult (RS_MULT)    | 2            |
| `div`  | Divisão inteira      | Mult (RS_MULT)    | 4            |
| `lw`   | Load word            | Load (RS_LOAD)    | 2            |
| `sw`   | Store word           | Load (RS_LOAD)    | 2            |

---

## Saída do Programa

A cada ciclo o programa imprime um bloco com três seções:

### 1. Status geral das instruções

```
-= STATUS GERAL ATUAL =-
[0] add DECODE/RS Add1
[1] sub WAITING
[2] mul EX em Mult1 (falta 1 ciclos)
```

Cada instrução exibe seu estágio atual: `WAITING`, `DECODE/RS <estação>`, `EX em <estação> (falta N ciclos)`, `COMMIT` ou `DONE`.

### 2. Tabela de status das instruções (ciclos)

```
-= STATUS DAS INSTRUCOES =-
Instrucao | RS    | Issue | EX Ini | EX Fim | Write
add       | Add1  | 1     | 2      | 2      | 2
sub       | Add2  | 1     | -1     | -1     | -1
mul       | Mult1 | 1     | 2      | 3      | 3
```

Valores `-1` indicam que a etapa ainda não ocorreu.

### 3. Estações de reserva

```
-= ESTACOES DE RESERVA =-
Nome  | Busy | Op  | Vj   | Vk   | Qj    | Qk    | A | EX | Status
Add1  | sim  | add | 10   | 20   | -     | -     | 0 | 1  | Pronta
Add2  | nao  | -   | 0    | 0    | -     | -     | 0 | 0  | -
Mult1 | sim  | mul | 0    | 0    | Add1  | -     | 0 | 2  | Esperando
```

Os campos seguem o padrão do algoritmo de Tomasulo:

| Campo | Descrição |
|-------|-----------|
| `Busy` | Indica se a estação está ocupada |
| `Op` | Operação alocada |
| `Vj` / `Vk` | Valores dos operandos (quando disponíveis) |
| `Qj` / `Qk` | Tags do produtor esperado (quando operando ainda não disponível) |
| `A` | Offset de endereço (para `lw`/`sw`) |
| `EX` | Ciclos restantes de execução |
| `Status` | `Pronta` (operandos disponíveis), `Esperando` (aguardando CDB) ou `-` (livre) |

### 4. Banco de registradores

```
-= BANCO DE REGISTRADORES =-
t0 | Valor: 0  | Qi: -
t1 | Valor: 10 | Qi: Add1
t2 | Valor: 20 | Qi: -
```

O campo `Qi` indica qual estação de reserva produzirá o próximo valor do registrador. Quando `Qi = -`, o valor atual é válido.

---

## Estrutura do Código

O simulador está inteiramente contido no arquivo `superescalar.c`, organizado nas seguintes seções:

### Constantes e configurações

```c
#define NUM_ADD_STATIONS  3   // estações para add, sub, or, and
#define NUM_MULT_STATIONS 2   // estações para mul, div
#define NUM_LOAD_STATIONS 2   // estações para lw, sw
#define NUM_REGISTERS     10  // registradores t0–t9
#define MAX_INSTRUCTIONS  100
```

### Estruturas de dados

| Estrutura | Descrição |
|-----------|-----------|
| `Register` | Registrador: nome, valor atual e campo `Qi` (tag do produtor pendente) |
| `Instruction` | Instrução: opcode, operandos, ciclos de cada estágio, estação alocada |
| `ReservationStation` | Estação de reserva: busy, op, Vj, Vk, Qj, Qk, offset, ciclos restantes |

### Estágios da instrução

```c
#define STAGE_WAIT    -1  // aguardando estação livre
#define STAGE_DECODE   0  // alocada em estação de reserva (Issue)
#define STAGE_EX       1  // em execução
#define STAGE_COMMIT   2  // resultado escrito no CDB (Write Result)
#define STAGE_DONE     3  // concluída
```

### Funções principais

| Função | Descrição |
|--------|-----------|
| `run_pipeline()` | Laço principal: executa ciclos até todas as instruções chegarem a `STAGE_DONE` |
| `reserve_station_for_instruction()` | Emite instrução para uma estação livre (fase Issue) |
| `reservation_station_operands_ready()` | Verifica se `Qj` e `Qk` estão ambos em `-` |
| `calculate_station_result()` | Calcula o resultado da operação com `Vj` e `Vk` |
| `broadcast_cdb()` | Transmite resultado pelo CDB, atualizando registradores e estações dependentes |
| `free_reservation_station()` | Libera a estação após o write result |
| `set_station_operand()` | Preenche `Vj`/`Vk` ou `Qj`/`Qk` conforme disponibilidade do operando |

---

## Detalhes de Implementação

### Superescalaridade: emissão e execução em paralelo

O simulador processa **até 2 instruções por ciclo** em cada fase:

- **Issue:** até 2 instruções em `STAGE_WAIT` são alocadas em estações livres no mesmo ciclo;
- **Execute:** até 2 instruções em `STAGE_DECODE` com operandos prontos iniciam execução simultaneamente;
- **Write Result:** até 2 instruções que terminaram execução escrevem no CDB no mesmo ciclo.

### Resolução de dependências RAW

Durante o Issue, `set_station_operand()` consulta o arquivo de registradores:

- Se `Qi == "-"` → operando disponível; copia o valor para `Vj` ou `Vk`;
- Se `Qi` aponta para uma estação → operando pendente; armazena a tag em `Qj` ou `Qk`.

A instrução só avança para execução quando ambos `Qj` e `Qk` forem `"-"`.

### Common Data Bus (CDB)

Ao concluir execução, `broadcast_cdb()` é chamado com o nome da estação e o resultado. Ele:

1. Atualiza o valor do registrador destino (`update_register_from_cdb`) e limpa seu `Qi`;
2. Varre todas as estações ocupadas e, para cada `Qj` ou `Qk` que coincida com a tag transmitida, copia o valor e limpa o campo — desbloqueando instruções dependentes.

### Eliminação de hazards WAR e WAW

A renomeação implícita pelas tags das estações de reserva elimina os hazards WAR e WAW sem necessidade de stall: duas escritas no mesmo registrador geram tags diferentes; leituras capturam o valor disponível no momento do Issue.

### Tratamento de `sw`

O `sw` não escreve em registrador, logo `instruction_writes_register()` retorna 0 para ele e `set_register_qi()` não é chamado. O dado a armazenar é tratado como operando `j` e o endereço base como operando `k`.

### Divisão por zero

Em `calculate_station_result()`, uma divisão com `Vk == 0` retorna 0 sem falha de execução.

---

## Casos de Teste

Os arquivos na pasta `./programa/` cobrem os cenários principais do algoritmo:

| Arquivo | Cenário testado |
|---------|-----------------|
| `test_dependencias.txt` | Dependências RAW em cadeia (instruções aguardam resultado de anteriores) |
| `test_renomeacao.txt` | Hazards WAW e WAR resolvidos por renomeação de registradores |
| `test_estacoes_cheias.txt` | Emissão bloqueada por falta de estação livre |
| `test_memoria.txt` | Instruções `lw` e `sw` com endereçamento `offset(base)` |

Para criar novos testes, basta adicionar um arquivo `.txt` seguindo o formato de entrada descrito acima.

---

## Referências

- HENNESSY, J. L.; PATTERSON, D. A. *Arquitetura de Computadores: Uma Abordagem Quantitativa*. 5ª ed. Elsevier, 2012. Cap. 3 e Apêndice C.
- TOMASULO, R. M. An Efficient Algorithm for Exploiting Multiple Arithmetic Units. *IBM Journal of Research and Development*, v. 11, n. 1, p. 25–33, 1967.
- Slides de Superescalaridade — disciplina de Arquitetura de Computadores.
