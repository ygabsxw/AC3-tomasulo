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
typedef struct
{
    char name[10];
    int value;
    char Qi[10];

} Register;

Register registers[NUM_REGISTERS];

typedef struct
{
    char op[10];

    char rd[10];
    char rs[10];
    char rt[10];

    int ex_cycles;
    int remaining_ex_cycles;

    int stage;

} Instruction;

Instruction instructions[MAX_INSTRUCTIONS];

int total_instructions = 0;

/* ----------------------------------- */
/* DEFINE QUANTOS CICLOS CADA OP GASTA */
/* ----------------------------------- */

int get_ex_cycles(char op[])
{
    // exceções
    if (strcmp(op, "mul") == 0)
        return 2;

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
        Instruction *inst;                /* remove o \n da linha */

        line[strcspn(line, "\n")] = '\0'; /* ignora linha vazia */

        if (strlen(line) == 0)
            continue;
        
        inst = &instructions[total_instructions]; 
        
        /* limpa os registradores */
        strcpy(inst->rd, "-");
        strcpy(inst->rs, "-");
        strcpy(inst->rt, "-"); 

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
            printf("DECODE");

        else if (instructions[i].stage == STAGE_EX)
            printf("EX (falta %d ciclos)", instructions[i].remaining_ex_cycles);

        else if (instructions[i].stage == STAGE_COMMIT)
            printf("COMMIT");

        else if (instructions[i].stage == STAGE_DONE)
            printf("DONE");

        printf("\n");
    }

    print_registers();
}

/* ---------------------- */
/* COMMIT  -> DONE        */
/* ---------------------- */
void empty_commits(){
    for(int i = 0; i < total_instructions; i++){
        if (instructions[i].stage == STAGE_COMMIT){
            instructions[i].stage = STAGE_DONE;
        }
    }
}

/* ----------------------------------- */
/* VERIFICA SE TERMINOU                */
/* ----------------------------------- */

int finished_program()
{
    //esvaziar estágio de commit (cabe no máximo 2)
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

    int decode_index = 0;

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
                instructions[i].remaining_ex_cycles--;

                //se houver espaço para despachar (max 2)
                if (instructions[i].remaining_ex_cycles <= 0 && commits < 2)
                {
                    instructions[i].stage = STAGE_COMMIT;
                    commits++;
                    printf("%s DESPACHADA\n", instructions[i].op);
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
            if (instructions[i].stage == STAGE_DECODE)
            {
                instructions[i].stage = STAGE_EX;

                printf("%s -> iniciou EX\n", instructions[i].op);

                executing++;
            }
        }

        /* ---------------------- */
        /* WAIT -> FETCH / DECODE */
        /* ---------------------- */

        int decoded = 0;

        // obter total de instruções atualmente decodificadas
        for (int i = 0; i < total_instructions; i++)
        {
            if (instructions[i].stage == STAGE_DECODE)
                decoded++;
        }

        // se houver espaço, decodificar até 2 novas instruções
        for (int i = 0; i < total_instructions && decoded < 2; i++)
        {
            if (instructions[i].stage == STAGE_WAIT)
            {
                instructions[i].stage = STAGE_DECODE;

                printf("%s -> foi decodificada\n", instructions[i].op);

                decoded++;
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

char* get_register_qi(char name[])
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
/* MAIN                                */
/* ----------------------------------- */

int main()
{
    initialize_registers();

    /* -------------------------- */
    /* USANDO APENAS PARA TESTES  */
    /* -------------------------- */
    printf("\nTESTE INICIAL DOS REGISTRADORES:\n");
    print_registers();

    printf("\nTESTE ALTERACAO DO QI:\n");
    set_register_qi("t1", "Add1");
    set_register_qi("t4", "Mul1");
    printf("Indice de t1: %d\n", find_register_index("t1"));
    printf("t1 esta pronto? %d\n", is_register_ready("t1"));
    printf("Valor de t2: %d\n", get_register_value("t2"));
    printf("Qi de t4: %s\n", get_register_qi("t4"));
    print_registers();

    printf("\nTESTE CDB:\n");
    update_register_from_cdb("Add1", 999);
    printf("t1 esta pronto? %d\n", is_register_ready("t1"));
    print_registers();
    /* -------------------------- */
    /* USANDO APENAS PARA TESTES  */
    /* -------------------------- */

    read_file("./programa/test.txt");

    run_pipeline();

    return 0;
}
