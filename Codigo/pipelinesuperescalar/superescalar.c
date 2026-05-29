#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INSTRUCTIONS 100

#define STAGE_WAIT -1
#define STAGE_DECODE 0
#define STAGE_EX 1
#define STAGE_COMMIT 2
#define STAGE_DONE 3

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
/* MAIN                                */
/* ----------------------------------- */

int main()
{
    read_file("./programa/test.txt");

    run_pipeline();

    return 0;
}
