#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INSTRUCTIONS 100

#define STAGE_WAIT -1
#define STAGE_DECODE 0
#define STAGE_EX 1
#define STAGE_COMMIT 2
#define STAGE_DONE 3
#define NUM_REGISTERS 10
#define NUM_ADD_STATIONS 3
#define NUM_MULT_STATIONS 2
#define NUM_LOAD_STATIONS 2
#define NUM_RESERVATION_STATIONS (NUM_ADD_STATIONS + NUM_MULT_STATIONS + NUM_LOAD_STATIONS)
typedef struct
{
    char name[10]; // nome do registrador
    int value;     // valor atual
    char Qi[10];   // estacao que vai produzir o valor

} Register;

Register registers[NUM_REGISTERS];

typedef struct
{
    char op[10]; // operacao da instrucao

    char rd[10]; // registrador destino
    char rs[10]; // primeiro operando
    char rt[10]; // segundo operando

    int ex_cycles;           // total de ciclos de execucao
    int remaining_ex_cycles; // ciclos restantes

    int stage;        // estado atual da instrucao
    char station[10]; // estacao de reserva usada

    /* ciclos em que a instrucao passou por cada etapa */
    int issue_cycle;
    int execute_start_cycle;
    int execute_end_cycle;
    int write_result_cycle;

} Instruction;

Instruction instructions[MAX_INSTRUCTIONS];

int total_instructions = 0;

typedef enum
{
    RS_ADD,  // add/sub
    RS_MULT, // mul/div
    RS_LOAD  // lw/sw
} ReservationStationType;

typedef struct
{
    char name[10];               // nome da estacao
    ReservationStationType type; // tipo da estacao
    int busy;                    // indica se esta ocupada

    char op[10];           // operacao alocada
    int instruction_index; // indice da instrucao
    char dest[10];         // registrador destino

    int Vj;      // valor do primeiro operando
    int Vk;      // valor do segundo operando
    char Qj[10]; // produtor do primeiro operando
    char Qk[10]; // produtor do segundo operando

    int offset;              // campo A para lw/sw
    int remaining_ex_cycles; // ciclos restantes na estacao
} ReservationStation;

ReservationStation reservation_stations[NUM_RESERVATION_STATIONS];

int is_register_ready(char name[]);
int get_register_value(char name[]);
char *get_register_qi(char name[]);
void set_register_qi(char name[], char station_name[]);
void update_register_from_cdb(char station_name[], int value);
void initialize_reservation_stations();
void print_reservation_stations();
int reserve_station_for_instruction(Instruction *inst, int instruction_index);
int reservation_station_operands_ready(char station_name[]);
int calculate_station_result(char station_name[]);
void free_reservation_station(char station_name[]);
void broadcast_cdb(char station_name[], int value);
int find_reservation_station_index(char station_name[]);

/* ----------------------------------- */
/* DEFINE QUANTOS CICLOS CADA OP GASTA */
/* ----------------------------------- */

int get_ex_cycles(char op[])
{
    // exceções
    if (strcmp(op, "mul") == 0)
        return 2;

    if (strcmp(op, "div") == 0)
        return 4;

    if (strcmp(op, "lw") == 0 || strcmp(op, "sw") == 0)
        return 2;

    return 1; // padrão
}

/* ------------------------- */
/* LEITURA DO ARQUIVO        */
/* ------------------------- */

void read_file(char filename[])
{
    FILE *file;
    char line[100];

    file = fopen(filename, "r");
    if (file == NULL)
    {
        printf("Erro ao abrir arquivo\n");
        return;
    }

    while (fgets(line, sizeof(line), file) != NULL)
    {
        Instruction *inst; /* remove o \n da linha */

        line[strcspn(line, "\n")] = '\0'; /* ignora linha vazia */

        if (strlen(line) == 0)
            continue;

        inst = &instructions[total_instructions];

        /* limpa os registradores */
        strcpy(inst->rd, "-");
        strcpy(inst->rs, "-");
        strcpy(inst->rt, "-");
        strcpy(inst->station, "-");

        /* -------------------------------- */
        /* FORMATO NORMAL */
        /* add t1, t2, t3 */
        /* -------------------------------- */

        if (sscanf(line, "%s %[^,], %[^,], %s", inst->op, inst->rd, inst->rs, inst->rt) == 4)
        {
            /* leitura feita com sucesso */
        }

        /* -------------------------------- */
        /* FORMATO LW/SW */ /* lw t0, 0(t1) */
        /* -------------------------------- */
        else if (sscanf(line, "%s %[^,], %s", inst->op, inst->rd, inst->rs) == 3)
        {
            /* rt permanece "-" */
        }
        else
        {
            printf("Erro ao ler instrucao:\n%s\n", line);
            continue;
        }

        inst->ex_cycles = get_ex_cycles(inst->op);
        inst->remaining_ex_cycles = inst->ex_cycles;

        inst->stage = STAGE_WAIT;

        /* ainda nao passou por nenhuma etapa */
        inst->issue_cycle = -1;
        inst->execute_start_cycle = -1;
        inst->execute_end_cycle = -1;
        inst->write_result_cycle = -1;

        total_instructions++;
    }
    fclose(file);
}

/* ----------------------------------- */
/* MOSTRA ESTADO DAS INSTRUCOES        */
/* ----------------------------------- */

void print_registers()
{
    int i;

    printf("\n-= BANCO DE REGISTRADORES =-\n");

    for (i = 0; i < NUM_REGISTERS; i++)
    {
        printf("%s | Valor: %d | Qi: %s\n",
               registers[i].name,
               registers[i].value,
               registers[i].Qi);
    }
}

void print_instruction_status()
{
    printf("\n-= STATUS DAS INSTRUCOES =-\n");

    printf("Instrucao              | RS    | Issue | EX Ini | EX Fim | Write\n");

    for (int i = 0; i < total_instructions; i++)
    {
        char full_instruction[50];

        if (strcmp(instructions[i].rt, "-") == 0)
        {
            sprintf(full_instruction, "%s %s, %s",
                    instructions[i].op,
                    instructions[i].rd,
                    instructions[i].rs);
        }
        else
        {
            sprintf(full_instruction, "%s %s, %s, %s",
                    instructions[i].op,
                    instructions[i].rd,
                    instructions[i].rs,
                    instructions[i].rt);
        }

        printf("%-22s | %-5s | %-5d | %-6d | %-6d | %-5d\n",
               full_instruction,
               instructions[i].station,
               instructions[i].issue_cycle,
               instructions[i].execute_start_cycle,
               instructions[i].execute_end_cycle,
               instructions[i].write_result_cycle);
    }

    printf("Instrucao              | RS    | Issue | EX Ini | EX Fim | Write\n");

    for (int i = 0; i < total_instructions; i++)
    {
        char full_instruction[50];

        if (strcmp(instructions[i].rt, "-") == 0)
        {
            sprintf(full_instruction, "%s %s, %s",
                    instructions[i].op,
                    instructions[i].rd,
                    instructions[i].rs);
        }
        else
        {
            sprintf(full_instruction, "%s %s, %s, %s",
                    instructions[i].op,
                    instructions[i].rd,
                    instructions[i].rs,
                    instructions[i].rt);
        }

        printf("%-22s | %-5s | %-5d | %-6d | %-6d | %-5d\n",
               full_instruction,
               instructions[i].station,
               instructions[i].issue_cycle,
               instructions[i].execute_start_cycle,
               instructions[i].execute_end_cycle,
               instructions[i].write_result_cycle);
    }
}

void print_state()
{
    int i;

    printf("\n-= STATUS GERAL ATUAL =-\n");
    for (i = 0; i < total_instructions; i++)
    {
        printf("[%d] %s ", i, instructions[i].op);
        if (instructions[i].stage == STAGE_WAIT)
            printf("WAITING");

        if (instructions[i].stage == STAGE_DECODE)
            printf("DECODE/RS %s", instructions[i].station);

        else if (instructions[i].stage == STAGE_EX)
            printf("EX em %s (falta %d ciclos)", instructions[i].station, instructions[i].remaining_ex_cycles);

        else if (instructions[i].stage == STAGE_COMMIT)
            printf("COMMIT");

        else if (instructions[i].stage == STAGE_DONE)
            printf("DONE");

        printf("\n");
    }

    print_instruction_status();
    print_reservation_stations();
    print_registers();
}

/* ---------------------- */
/* COMMIT  -> DONE        */
/* ---------------------- */
void empty_commits()
{
    for (int i = 0; i < total_instructions; i++)
    {
        if (instructions[i].stage == STAGE_COMMIT)
        {
            instructions[i].stage = STAGE_DONE;
        }
    }
}

/* ----------------------------------- */
/* VERIFICA SE TERMINOU                */
/* ----------------------------------- */

int finished_program()
{
    // esvaziar estágio de commit (cabe no máximo 2)
    empty_commits();

    for (int i = 0; i < total_instructions; i++)
    {
        if (instructions[i].stage != STAGE_DONE)
            return 0;
    }

    return 1;
}

/* ----------------------------------- */
/* PIPELINE                            */
/* ----------------------------------- */

void run_pipeline()
{
    int cycle = 1;

    while (!finished_program())
    {
        printf("\n====================\n");
        printf("CICLO %d\n", cycle);
        printf("====================\n");

        /* ---------------------- */
        /* ESTÁGIO DE EXECUCAO    */
        /* ---------------------- */

        int commits = 0;
        for (int i = 0; i < total_instructions; i++)
        {
            if (instructions[i].stage == STAGE_EX)
            {   
                int station_index = find_reservation_station_index(instructions[i].station);

                if (station_index == -1)
                    continue;

                instructions[i].remaining_ex_cycles--;
                reservation_stations[station_index].remaining_ex_cycles = instructions[i].remaining_ex_cycles;
                // se houver espaço para despachar (max 2)
                if (instructions[i].remaining_ex_cycles <= 0 && commits < 2)
                {
                    /* registra quando a execucao terminou */
                    instructions[i].execute_end_cycle = cycle;

                    /* registra quando o resultado foi escrito */
                    instructions[i].write_result_cycle = cycle;
                    
                    int result = calculate_station_result(instructions[i].station);
                    broadcast_cdb(instructions[i].station, result);
                    free_reservation_station(instructions[i].station);
                    instructions[i].stage = STAGE_COMMIT;
                    commits++;
                    printf("%s DESPACHADA PELO CDB (%s = %d)\n",
                           instructions[i].op,
                           instructions[i].station,
                           result);
                }
            }
        }

        /* ---------------------- */
        /* DISPATCH PARA EX       */
        /* ---------------------- */

        int executing = 0;

        // obter total de instruções atualmente decodificadas
        for (int i = 0; i < total_instructions; i++)
        {
            if (instructions[i].stage == STAGE_EX)
                executing++;
        }

        // se houver espaço, colocar mais instruções em execução (até 2)
        for (int i = 0; i < total_instructions && executing < 2; i++)
        {
            if (instructions[i].stage == STAGE_DECODE &&
                reservation_station_operands_ready(instructions[i].station))
            {
                instructions[i].stage = STAGE_EX;

                /* registra quando a instrucao iniciou execucao */
                instructions[i].execute_start_cycle = cycle;

                printf("%s -> iniciou EX\n", instructions[i].op);

                executing++;
            }
        }

        /* ---------------------- */
        /* WAIT -> FETCH / DECODE */
        /* ---------------------- */

        int issued = 0;

        // se houver estação livre, alocar até 2 instruções por ciclo
        for (int i = 0; i < total_instructions && issued < 2; i++)
        {
            if (instructions[i].stage == STAGE_WAIT)
            {
                int station_index = reserve_station_for_instruction(&instructions[i], i);

                if (station_index == -1)
                    break;

                instructions[i].stage = STAGE_DECODE;

                /* registra o ciclo em que a instrucao entrou no sistema */
                instructions[i].issue_cycle = cycle;

                printf("%s -> alocada na estacao %s\n",
                       instructions[i].op,
                       instructions[i].station);

                issued++;
            }
        }

        /* ---------------------- */

        print_state();

        cycle++;
    }
}

/* ----------------------------------- */
/* INICIALIZA REGISTRADORES            */
/* ----------------------------------- */

void initialize_registers()
{
    int i;

    for (i = 0; i < NUM_REGISTERS; i++)
    {
        sprintf(registers[i].name, "t%d", i);
        registers[i].value = i * 10;
        strcpy(registers[i].Qi, "-");
    }
}

/* ----------------------------------------- */
/* ENCONTRA ÍNDICE DO REGISTRADOR PELO NOME  */
/* ----------------------------------------- */

int find_register_index(char name[])
{
    for (int i = 0; i < NUM_REGISTERS; i++)
    {
        if (strcmp(registers[i].name, name) == 0)
            return i;
    }
    return -1; // não encontrado
}

/* ----------------------------------- */
/* VERIFICA SE REGISTRADOR ESTÁ PRONTO */
/* ----------------------------------- */

int is_register_ready(char name[])
{
    int index = find_register_index(name);

    if ((index != -1) && (strcmp(registers[index].Qi, "-") == 0))
        return 1;
    else
        return 0;
}

/* ----------------------------------- */
/* OBTÉM O VALOR DO REGISTRADOR        */
/* ----------------------------------- */

int get_register_value(char name[])
{
    int index = find_register_index(name);
    if (index == -1)
        return 0; // se não for registrador, retorna 0
    return registers[index].value;
}

/* ----------------------------------- */
/* OBTÉM O QI DO REGISTRADOR           */
/* ----------------------------------- */

char *get_register_qi(char name[])
{
    int index = find_register_index(name);
    if (index == -1)
        return "-"; // se não for registrador, retorna "-"
    return registers[index].Qi;
}

/* ----------------------------------- */
/* ALTERA O QI DO REGISTRADOR          */
/* ----------------------------------- */

void set_register_qi(char name[], char station_name[])
{
    int index = find_register_index(name);
    if (index != -1)
        strcpy(registers[index].Qi, station_name);
}

/* ----------------------------------- */
/* ATUALIZA REGISTRADOR PELO CDB       */
/* ----------------------------------- */

void update_register_from_cdb(char station_name[], int value)
{
    for (int i = 0; i < NUM_REGISTERS; i++)
    {
        if (strcmp(registers[i].Qi, station_name) == 0)
        {
            registers[i].value = value;
            strcpy(registers[i].Qi, "-");
        }
    }
}

/* ----------------------------------- */
/* ESTACOES DE RESERVA                 */
/* ----------------------------------- */

/* ----------------------------------- */
/* LIMPA UMA ESTACAO DE RESERVA        */
/* ----------------------------------- */

void clear_reservation_station(ReservationStation *station)
{
    station->busy = 0;
    strcpy(station->op, "-");
    station->instruction_index = -1;
    strcpy(station->dest, "-");
    station->Vj = 0;
    station->Vk = 0;
    strcpy(station->Qj, "-");
    strcpy(station->Qk, "-");
    station->offset = 0;
    station->remaining_ex_cycles = 0;
}

/* ----------------------------------- */
/* INICIALIZA ESTACOES DE RESERVA      */
/* ----------------------------------- */

void initialize_reservation_stations()
{
    int index = 0;

    for (int i = 0; i < NUM_ADD_STATIONS; i++)
    {
        sprintf(reservation_stations[index].name, "Add%d", i + 1);
        reservation_stations[index].type = RS_ADD;
        clear_reservation_station(&reservation_stations[index]);
        index++;
    }

    for (int i = 0; i < NUM_MULT_STATIONS; i++)
    {
        sprintf(reservation_stations[index].name, "Mult%d", i + 1);
        reservation_stations[index].type = RS_MULT;
        clear_reservation_station(&reservation_stations[index]);
        index++;
    }

    for (int i = 0; i < NUM_LOAD_STATIONS; i++)
    {
        sprintf(reservation_stations[index].name, "Load%d", i + 1);
        reservation_stations[index].type = RS_LOAD;
        clear_reservation_station(&reservation_stations[index]);
        index++;
    }
}

/* ----------------------------------- */
/* VERIFICA SE A INSTRUCAO ESCREVE REG */
/* ----------------------------------- */

int instruction_writes_register(char op[])
{
    return strcmp(op, "sw") != 0;
}

/* ----------------------------------- */
/* VERIFICA SE ESTACAO ACEITA OPERACAO */
/* ----------------------------------- */

int station_accepts_operation(ReservationStationType type, char op[])
{
    if (type == RS_ADD)
        return strcmp(op, "add") == 0 || strcmp(op, "sub") == 0
        || strcmp(op,"or") == 0 || strcmp(op, "and") == 0;

    if (type == RS_MULT)
        return strcmp(op, "mul") == 0 || strcmp(op, "div") == 0;

    if (type == RS_LOAD)
        return strcmp(op, "lw") == 0 || strcmp(op, "sw") == 0;

    return 0;
}

/* ----------------------------------- */
/* BUSCA ESTACAO PELO NOME             */
/* ----------------------------------- */

int find_reservation_station_index(char station_name[])
{
    for (int i = 0; i < NUM_RESERVATION_STATIONS; i++)
    {
        if (strcmp(reservation_stations[i].name, station_name) == 0)
            return i;
    }

    return -1;
}

/* ----------------------------------- */
/* BUSCA ESTACAO LIVRE PARA OPERACAO   */
/* ----------------------------------- */

int find_free_reservation_station(char op[])
{
    for (int i = 0; i < NUM_RESERVATION_STATIONS; i++)
    {
        if (!reservation_stations[i].busy &&
            station_accepts_operation(reservation_stations[i].type, op))
            return i;
    }

    return -1;
}

/* ----------------------------------- */
/* PREENCHE OPERANDO DA ESTACAO        */
/* ----------------------------------- */

void set_station_operand(ReservationStation *station, char operand, char register_name[])
{
    int value = 0;
    char producer[10] = "-";

    if (strcmp(register_name, "-") != 0)
    {
        if (register_name[0] >= '0' && register_name[0] <= '9')
        {
            value = atoi(register_name); 
        }

        else{ 
            
        if (is_register_ready(register_name))
            value = get_register_value(register_name);
        else
            strcpy(producer, get_register_qi(register_name));
        }
    }

    if (operand == 'j')
    {
        station->Vj = value;
        strcpy(station->Qj, producer);
    }
    else
    {
        station->Vk = value;
        strcpy(station->Qk, producer);
    }
}

/* ----------------------------------- */
/* SEPARA DESLOCAMENTO E REG BASE      */
/* ----------------------------------- */

int parse_memory_operand(char operand[], int *offset, char base_register[])
{
    if (sscanf(operand, "%d(%9[^)])", offset, base_register) == 2)
        return 1;

    *offset = 0;
    strcpy(base_register, "-");
    return 0;
}

/* ----------------------------------- */
/* ALOCA INSTRUCAO EM ESTACAO          */
/* ----------------------------------- */

int reserve_station_for_instruction(Instruction *inst, int instruction_index)
{
    int station_index = find_free_reservation_station(inst->op);
    ReservationStation *station;

    if (station_index == -1)
        return -1;

    station = &reservation_stations[station_index];

    station->busy = 1;
    strcpy(station->op, inst->op);
    station->instruction_index = instruction_index;
    strcpy(station->dest, inst->rd);
    station->remaining_ex_cycles = inst->ex_cycles;
    station->offset = 0;

    strcpy(station->Qj, "-");
    strcpy(station->Qk, "-");
    station->Vj = 0;
    station->Vk = 0;

    if (strcmp(inst->op, "lw") == 0)
    {
        char base_register[10];
        parse_memory_operand(inst->rs, &station->offset, base_register);
        set_station_operand(station, 'j', base_register);
    }
    else if (strcmp(inst->op, "sw") == 0)
    {
        char base_register[10];
        parse_memory_operand(inst->rs, &station->offset, base_register);
        set_station_operand(station, 'j', inst->rd);
        set_station_operand(station, 'k', base_register);
    }
    else
    {
        set_station_operand(station, 'j', inst->rs);
        set_station_operand(station, 'k', inst->rt);
    }

    strcpy(inst->station, station->name);

    if (instruction_writes_register(inst->op))
        set_register_qi(inst->rd, station->name);

    return station_index;
}

/* ----------------------------------- */
/* VERIFICA SE OPERANDOS ESTAO PRONTOS */
/* ----------------------------------- */

int reservation_station_operands_ready(char station_name[])
{
    int index = find_reservation_station_index(station_name);

    if (index == -1 || !reservation_stations[index].busy)
        return 0;

    return strcmp(reservation_stations[index].Qj, "-") == 0 &&
           strcmp(reservation_stations[index].Qk, "-") == 0;
}

/* ----------------------------------- */
/* CALCULA RESULTADO DA ESTACAO        */
/* ----------------------------------- */

int calculate_station_result(char station_name[])
{
    int index = find_reservation_station_index(station_name);
    ReservationStation *station;

    if (index == -1)
        return 0;

    station = &reservation_stations[index];

    if (strcmp(station->op, "add") == 0)
        return station->Vj + station->Vk;

    if (strcmp(station->op, "sub") == 0)
        return station->Vj - station->Vk;

    if(strcmp(station->op, "or") == 0)
        return station->Vj | station->Vk;

    if(strcmp(station->op, "and") == 0)
        return station->Vj & station->Vk;

    if (strcmp(station->op, "mul") == 0)
        return station->Vj * station->Vk;

    if (strcmp(station->op, "div") == 0)
    {
        if (station->Vk == 0)
            return 0;

        return station->Vj / station->Vk;
    }

    if (strcmp(station->op, "lw") == 0 || strcmp(station->op, "sw") == 0)
        return station->Vj + station->offset;

    return 0;
}

/* ----------------------------------- */
/* ATUALIZA ESTACOES PELO CDB          */
/* ----------------------------------- */

void update_reservation_stations_from_cdb(char station_name[], int value)
{
    for (int i = 0; i < NUM_RESERVATION_STATIONS; i++)
    {
        if (!reservation_stations[i].busy)
            continue;

        if (strcmp(reservation_stations[i].Qj, station_name) == 0)
        {
            reservation_stations[i].Vj = value;
            strcpy(reservation_stations[i].Qj, "-");
        }

        if (strcmp(reservation_stations[i].Qk, station_name) == 0)
        {
            reservation_stations[i].Vk = value;
            strcpy(reservation_stations[i].Qk, "-");
        }
    }
}

/* ----------------------------------- */
/* LIBERA ESTACAO DE RESERVA           */
/* ----------------------------------- */

void free_reservation_station(char station_name[])
{
    int index = find_reservation_station_index(station_name);

    if (index != -1)
        clear_reservation_station(&reservation_stations[index]);
}

/* ----------------------------------- */
/* SIMULA BROADCAST DO CDB             */
/* ----------------------------------- */

void broadcast_cdb(char station_name[], int value)
{
    update_register_from_cdb(station_name, value);
    update_reservation_stations_from_cdb(station_name, value);
}

/* ----------------------------------- */
/* MOSTRA ESTACOES DE RESERVA          */
/* ----------------------------------- */

void print_reservation_stations()
{
    printf("\n-= ESTACOES DE RESERVA =-\n");
    // Padrão do slide 06, pag 36
    printf("Nome  | Busy | Op  | Vj   | Vk   | Qj    | Qk    | A | EX | Status\n");

    for (int i = 0; i < NUM_RESERVATION_STATIONS; i++)
    {
        ReservationStation *station = &reservation_stations[i];

        char status[20];

        if (strcmp(station->Qj, "-") != 0 || strcmp(station->Qk, "-") != 0)
            strcpy(status, "Esperando");
        else if (station->busy)
            strcpy(status, "Pronta");
        else
            strcpy(status, "-");

        printf("%-5s | %-4s | %-3s | %-4d | %-4d | %-5s | %-5s | %d | %d | %s\n",
               station->name,
               station->busy ? "sim" : "nao",
               station->busy ? station->op : "-",
               station->Vj,
               station->Vk,
               station->Qj,
               station->Qk,
               station->offset,
               station->remaining_ex_cycles,
               status);
    }
}

/* ----------------------------------- */
/* MAIN                                */
/* ----------------------------------- */

int main(int argc, char *argv[])
{
    char *input_file = "./programa/test.txt";

    if (argc > 1)
        input_file = argv[1];

    initialize_registers();
    initialize_reservation_stations();

    printf("Arquivo de entrada: %s\n", input_file);

    read_file(input_file);

    run_pipeline();

    return 0;
}

// gcc -Wall -Wextra -std=c11 superescalar.c -o superescalar
// ./superescalar ./programa/test_dependencias.txt
// ./superescalar ./programa/test_renomeacao.txt
// ./superescalar ./programa/test_estacoes_cheias.txt
// ./superescalar ./programa/test_memoria.txt
