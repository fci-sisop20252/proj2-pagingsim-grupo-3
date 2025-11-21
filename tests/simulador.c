/* simulador.c
 *
 * Simulador de Memória Virtual com Paginação
 * Suporta algoritmos: fifo e clock
 *
 * Uso:
 *   ./simulador <fifo|clock> <arquivo_config> <arquivo_acessos>
 *
 * (Implementado conforme especificação do ENUNCIADO.md)
 *
 * Autor: gerado por assistente (ajuste livre)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 256

/* Estruturas de dados */
typedef struct {
    int frame;      // índice do frame onde a página está (-1 se não estiver)
    int valid;      // V bit (0 ou 1)
    int referenced; // R bit (0 ou 1)
} PageEntry;

typedef struct {
    int pid;
    int virtual_size; // em bytes
    int num_pages;
    PageEntry *pages; // vetor de PageEntry com tamanho num_pages
} Process;

typedef struct {
    int occupied; // 0 livre, 1 ocupado
    int pid;      // pid da página que está neste frame
    int page;     // número da página
    int referenced; // R bit duplicado aqui para acesso rápido (mantenha em sincronia com tabela de páginas)
} Frame;

/* FIFO queue (armazenamos índices de frames em ordem de chegada) */
typedef struct {
    int *data;
    int capacity;
    int head;
    int tail;
    int size;
} Queue;

/* Protótipos */
Queue *queue_create(int capacity);
void queue_destroy(Queue *q);
int queue_push(Queue *q, int value);
int queue_pop(Queue *q, int *out);
int queue_is_empty(Queue *q);

Process *find_process(Process *processes, int nproc, int pid);
void free_processes(Process *processes, int nproc);

/* Globals para a simulação */
Frame *frames = NULL;
int nframes = 0;
int page_size = 0;
Process *processes = NULL;
int nproc = 0;

Queue *fifo_queue = NULL; // usada no FIFO
int clock_hand = 0;       // ponteiro do algoritmo Clock

/* Funções utilitárias */
Queue *queue_create(int capacity) {
    Queue *q = malloc(sizeof(Queue));
    q->data = malloc(sizeof(int) * capacity);
    q->capacity = capacity;
    q->head = 0;
    q->tail = 0;
    q->size = 0;
    return q;
}
void queue_destroy(Queue *q) {
    if (!q) return;
    free(q->data);
    free(q);
}
int queue_push(Queue *q, int value) {
    if (q->size == q->capacity) return 0;
    q->data[q->tail] = value;
    q->tail = (q->tail + 1) % q->capacity;
    q->size++;
    return 1;
}
int queue_pop(Queue *q, int *out) {
    if (q->size == 0) return 0;
    if (out) *out = q->data[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->size--;
    return 1;
}
int queue_is_empty(Queue *q) {
    return q->size == 0;
}

Process *find_process(Process *processes, int nproc, int pid) {
    for (int i = 0; i < nproc; ++i) {
        if (processes[i].pid == pid) return &processes[i];
    }
    return NULL;
}

void free_processes(Process *processes, int nproc) {
    if (!processes) return;
    for (int i = 0; i < nproc; ++i) {
        free(processes[i].pages);
    }
    free(processes);
}

/* Inicializações */
int init_from_config(const char *config_file) {
    FILE *f = fopen(config_file, "r");
    if (!f) {
        fprintf(stderr, "Erro: nao foi possivel abrir %s\n", config_file);
        return 0;
    }
    char line[MAX_LINE];
    // Le a primeira linha: numero_de_frames
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }
    while (line[0] == '\n') if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }
    nframes = atoi(line);

    // Segunda linha: tamanho_da_pagina
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }
    page_size = atoi(line);

    // Terceira linha: numero_de_processos
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }
    nproc = atoi(line);

    processes = malloc(sizeof(Process) * nproc);
    for (int i = 0; i < nproc; ++i) {
        if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }
        // pular linhas vazias
        while (line[0] == '\n') if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }
        // formato: <PID> <tamanho_memoria_virtual>
        int pid, vsize;
        if (sscanf(line, "%d %d", &pid, &vsize) != 2) {
            fprintf(stderr, "Formato invalido em config (linha de processos)\n");
            fclose(f);
            return 0;
        }
        processes[i].pid = pid;
        processes[i].virtual_size = vsize;
        processes[i].num_pages = (vsize + page_size - 1) / page_size; // ceil
        processes[i].pages = malloc(sizeof(PageEntry) * processes[i].num_pages);
        for (int p = 0; p < processes[i].num_pages; ++p) {
            processes[i].pages[p].frame = -1;
            processes[i].pages[p].valid = 0;
            processes[i].pages[p].referenced = 0;
        }
    }

    fclose(f);

    // Inicializar frames
    frames = malloc(sizeof(Frame) * nframes);
    for (int i = 0; i < nframes; ++i) {
        frames[i].occupied = 0;
        frames[i].pid = -1;
        frames[i].page = -1;
        frames[i].referenced = 0;
    }

    // Inicializar FIFO queue
    fifo_queue = queue_create(nframes);

    // clock hand inicia em 0
    clock_hand = 0;

    return 1;
}

/* Procura por um frame livre; retorna índice ou -1 se nenhum livre */
int find_free_frame() {
    for (int i = 0; i < nframes; ++i) {
        if (!frames[i].occupied) return i;
    }
    return -1;
}

/* Alocar pagina no frame especificado (atualiza frames e tabela de páginas) */
void allocate_page_to_frame(int frame_index, Process *proc, int page_num) {
    frames[frame_index].occupied = 1;
    frames[frame_index].pid = proc->pid;
    frames[frame_index].page = page_num;
    frames[frame_index].referenced = 1;

    proc->pages[page_num].frame = frame_index;
    proc->pages[page_num].valid = 1;
    proc->pages[page_num].referenced = 1;
}

/* Desaloca a pagina que estava em frame (atualiza tabela do processo que era dona) */
void deallocate_frame(int frame_index) {
    if (!frames[frame_index].occupied) return;
    int old_pid = frames[frame_index].pid;
    int old_page = frames[frame_index].page;
    // encontrar processo
    Process *p = find_process(processes, nproc, old_pid);
    if (p) {
        if (old_page >= 0 && old_page < p->num_pages) {
            p->pages[old_page].frame = -1;
            p->pages[old_page].valid = 0;
            p->pages[old_page].referenced = 0;
        }
    }
    frames[frame_index].occupied = 0;
    frames[frame_index].pid = -1;
    frames[frame_index].page = -1;
    frames[frame_index].referenced = 0;
}

/* Substituição FIFO: remove frame do head da fila */
int select_victim_fifo() {
    int victim_frame = -1;
    if (queue_pop(fifo_queue, &victim_frame)) {
        // antes de devolver, victim_frame deve estar ocupado; vamos desalocar fora da seleção
        return victim_frame;
    } else {
        // fila vazia (não deveria ocorrer se memoria cheia), tentamos buscar frame ocupado
        for (int i = 0; i < nframes; ++i) {
            if (frames[i].occupied) return i;
        }
        return -1;
    }
}

/* Seleção de vítima Clock (segunda chance).
   Retorna índice do frame vítima. */
int select_victim_clock() {
    // pressupõe que não existe frame livre
    int scanned = 0;
    while (1) {
        if (frames[clock_hand].occupied == 0) {
            // não deveria acontecer (checado antes), mas se ocorrer, esse é um frame livre
            int victim = clock_hand;
            clock_hand = (clock_hand + 1) % nframes;
            return victim;
        }
        if (frames[clock_hand].referenced == 0) {
            int victim = clock_hand;
            clock_hand = (clock_hand + 1) % nframes;
            return victim;
        } else {
            // dar segunda chance: zerar R nos dois lugares (frame e tabela de páginas)
            int old_pid = frames[clock_hand].pid;
            int old_page = frames[clock_hand].page;
            Process *p = find_process(processes, nproc, old_pid);
            if (p && old_page >= 0 && old_page < p->num_pages) {
                p->pages[old_page].referenced = 0;
            }
            frames[clock_hand].referenced = 0;
            clock_hand = (clock_hand + 1) % nframes;
        }
        scanned++;
        // como segurança (não infinit loop) se passaram 2 voltas, continue tentando
        if (scanned > nframes * 2) {
            // escolher o frame atual como vítima por segurança
            int victim = clock_hand;
            clock_hand = (clock_hand + 1) % nframes;
            return victim;
        }
    }
}

/* Trata um único acesso com algoritmo FIFO */
void handle_access_fifo(int pid, int address, int *total_faults) {
    Process *p = find_process(processes, nproc, pid);
    if (!p) {
        // processo desconhecido - ignorar ou imprimir erro
        // Impressão de erro em stderr para debug; não altera formato de saída
        fprintf(stderr, "Aviso: acesso de PID %d desconhecido (ignorando)\n", pid);
        return;
    }
    int page = address / page_size;
    int offset = address % page_size;
    // valida página
    if (page < 0 || page >= p->num_pages) {
        fprintf(stderr, "Aviso: endereco %d fora do espaco virtual do PID %d (pagina %d).\n", address, pid, page);
        // Ainda assim, tratar como page fault simples: porém preferimos apenas retornar
        return;
    }

    if (p->pages[page].valid) {
        int frame_index = p->pages[page].frame;
        // HIT: set R=1
        p->pages[page].referenced = 1;
        frames[frame_index].referenced = 1;
        printf("Acesso: PID %d, Endereço %d (Página %d, Deslocamento %d) -> HIT: Página %d (PID %d) já está no Frame %d\n",
               pid, address, page, offset, page, pid, frame_index);
        return;
    } else {
        // PAGE FAULT
        (*total_faults)++;
        int free_frame = find_free_frame();
        if (free_frame != -1) {
            // alocar no frame livre
            allocate_page_to_frame(free_frame, p, page);
            // adicionar ao final da fila FIFO
            queue_push(fifo_queue, free_frame);
            printf("Acesso: PID %d, Endereço %d (Página %d, Deslocamento %d) -> PAGE FAULT -> Página %d (PID %d) alocada no Frame livre %d\n",
                   pid, address, page, offset, page, pid, free_frame);
            return;
        } else {
            // Memória cheia: escolher vítima FIFO (head)
            int victim_frame = select_victim_fifo();
            // Detalhes da página vítima para a mensagem
            int old_pid = frames[victim_frame].pid;
            int old_page = frames[victim_frame].page;
            // desalocar vítima
            deallocate_frame(victim_frame);
            // alocar nova página no frame
            allocate_page_to_frame(victim_frame, p, page);
            // após substituir, este frame é considerado "novo", então adicionamos ao final da fila
            queue_push(fifo_queue, victim_frame);
            printf("Acesso: PID %d, Endereço %d (Página %d, Deslocamento %d) -> PAGE FAULT -> Memória cheia. Página %d (PID %d) (Frame %d) será desalocada. -> Página %d (PID %d) alocada no Frame %d\n",
                   pid, address, page, offset, old_page, old_pid, victim_frame, page, pid, victim_frame);
            return;
        }
    }
}

/* Trata um único acesso com algoritmo Clock */
void handle_access_clock(int pid, int address, int *total_faults) {
    Process *p = find_process(processes, nproc, pid);
    if (!p) {
        fprintf(stderr, "Aviso: acesso de PID %d desconhecido (ignorando)\n", pid);
        return;
    }
    int page = address / page_size;
    int offset = address % page_size;
    if (page < 0 || page >= p->num_pages) {
        fprintf(stderr, "Aviso: endereco %d fora do espaco virtual do PID %d (pagina %d).\n", address, pid, page);
        return;
    }

    if (p->pages[page].valid) {
        int frame_index = p->pages[page].frame;
        // HIT: set R=1 (tanto na tabela quanto no frame)
        p->pages[page].referenced = 1;
        frames[frame_index].referenced = 1;
        printf("Acesso: PID %d, Endereço %d (Página %d, Deslocamento %d) -> HIT: Página %d (PID %d) já está no Frame %d\n",
               pid, address, page, offset, page, pid, frame_index);
        return;
    } else {
        // PAGE FAULT
        (*total_faults)++;
        int free_frame = find_free_frame();
        if (free_frame != -1) {
            // alocar no frame livre
            allocate_page_to_frame(free_frame, p, page);
            // No Clock não precisamos de fila FIFO, mas o frame entrou na memória
            printf("Acesso: PID %d, Endereço %d (Página %d, Deslocamento %d) -> PAGE FAULT -> Página %d (PID %d) alocada no Frame livre %d\n",
                   pid, address, page, offset, page, pid, free_frame);
            return;
        } else {
            // Memória cheia: usar Clock para selecionar vítima
            int victim_frame = select_victim_clock();
            int old_pid = frames[victim_frame].pid;
            int old_page = frames[victim_frame].page;
            // desalocar vítima
            deallocate_frame(victim_frame);
            // alocar nova página no frame vítima
            allocate_page_to_frame(victim_frame, p, page);
            printf("Acesso: PID %d, Endereço %d (Página %d, Deslocamento %d) -> PAGE FAULT -> Memória cheia. Página %d (PID %d) (Frame %d) será desalocada. -> Página %d (PID %d) alocada no Frame %d\n",
                   pid, address, page, offset, old_page, old_pid, victim_frame, page, pid, victim_frame);
            return;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <fifo|clock> <arquivo_config> <arquivo_acessos>\n", argv[0]);
        return 1;
    }
    char algoritmo[16];
    strncpy(algoritmo, argv[1], sizeof(algoritmo)-1);
    algoritmo[sizeof(algoritmo)-1] = '\0';
    for (char *s = algoritmo; *s; ++s) *s = (char)tolower(*s);

    const char *config_file = argv[2];
    const char *access_file = argv[3];

    if (!init_from_config(config_file)) {
        fprintf(stderr, "Falha ao inicializar a partir do arquivo de configuracao.\n");
        return 1;
    }

    FILE *facc = fopen(access_file, "r");
    if (!facc) {
        fprintf(stderr, "Erro: nao foi possivel abrir %s\n", access_file);
        free_processes(processes, nproc);
        queue_destroy(fifo_queue);
        free(frames);
        return 1;
    }

    char line[MAX_LINE];
    int total_accesses = 0;
    int total_pagefaults = 0;

    while (fgets(line, sizeof(line), facc)) {
        // pular linhas em branco
        char *ptr = line;
        while (*ptr && isspace((unsigned char)*ptr)) ptr++;
        if (*ptr == '\0' || *ptr == '\n') continue;

        int pid, addr;
        if (sscanf(line, "%d %d", &pid, &addr) != 2) continue;

        total_accesses++;

        if (strcmp(algoritmo, "fifo") == 0) {
            handle_access_fifo(pid, addr, &total_pagefaults);
        } else if (strcmp(algoritmo, "clock") == 0) {
            handle_access_clock(pid, addr, &total_pagefaults);
        } else {
            fprintf(stderr, "Algoritmo desconhecido: %s (usar fifo ou clock)\n", algoritmo);
            fclose(facc);
            free_processes(processes, nproc);
            queue_destroy(fifo_queue);
            free(frames);
            return 1;
        }
    }

    fclose(facc);

    // Impressao do resumo final (exatamente no formato pedido)
    printf("--- Simulação Finalizada (Algoritmo: %s)\n", algoritmo);
    printf("Total de Acessos: %d\n", total_accesses);
    printf("Total de Page Faults: %d\n", total_pagefaults);

    // liberar memoria
    free_processes(processes, nproc);
    queue_destroy(fifo_queue);
    free(frames);

    return 0;
}

