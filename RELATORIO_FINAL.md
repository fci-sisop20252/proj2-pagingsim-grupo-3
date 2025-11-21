# Relatório do Projeto 2: Simulador de Memória Virtual

**Disciplina:** Sistemas Operacionais
**Professor:** Lucas Figueiredo
**Data:** 21/11

## Integrantes do Grupo

- Felipe Martha - 10437877
- Guillermo Martinez - 10418697
---

## 1. Instruções de Compilação e Execução

### 1.1 Compilação

Descreva EXATAMENTE como compilar seu projeto. Inclua todos os comandos necessários.

Comandos:
```
cd /workspaces/proj2-pagingsim-grupo-3/tests
gcc -o simulador simulador.c
```

### 1.2 Execução

Forneça exemplos completos de como executar o simulador.

**Exemplo com FIFO:**
```bash
./simulador fifo tests/config_1.txt tests/acessos_1.txt
```

**Exemplo com Clock:**
```bash
./simulador clock tests/config_1.txt tests/acessos_1.txt
```

---

## 2. Decisões de Design

### 2.1 Estruturas de Dados

Descreva as estruturas de dados que você escolheu para representar:

**Tabela de Páginas:**
- Qual estrutura usou? (array, lista, hash map, etc.)
Resposta: Array dinâmico de structs PageEntry dentro de cada struct Process
- Quais informações armazena para cada página?
Resposta: Número do frame, bit de validade (V) e bit de referência (R)
- Como organizou para múltiplos processos?
Resposta: Um array global de Process, onde cada processo possui seu próprio array pages indexado pelo número da página virtual
- **Justificativa:** Por que escolheu essa abordagem?
Resposta: Acesso direto O(1) pelo índice da página e isolamento simples da memória de cada processo

**Frames Físicos:**
- Como representou os frames da memória física?
Resposta: Array global de structs Frame
- Quais informações armazena para cada frame?
Resposta: Status de ocupação, PID do dono, número da página virtual mapeada e cópia do bit R
- Como rastreia frames livres vs ocupados?
Resposta: Campo inteiro occupied (0 para livre, 1 para ocupado)
- **Justificativa:** Por que escolheu essa abordagem?
Resposta: Permite mapeamento reverso rápido (saber qual página desalocar dado um frame) e acesso O(1) pelo índice do frame

**Estrutura para FIFO:**
- Como mantém a ordem de chegada das páginas?
Resposta: Fila circular (Circular Queue) implementada com array
- Como identifica a página mais antiga?
Resposta: O elemento apontado pelo índice head da fila
- **Justificativa:** Por que escolheu essa abordagem?
Resposta: Inserção e remoção em tempo constante O(1) sem necessidade de reordenar elementos

**Estrutura para Clock:**
- Como implementou o ponteiro circular?
Resposta: Variável inteira clock_hand que itera de 0 a nframes-1 usando operação de módulo (%)
- Como armazena e atualiza os R-bits?
Resposta: Armazenados na struct Frame para acesso rápido durante a varredura. Se o bit for 1, é zerado (segunda chance); se 0, é a vítima
- **Justificativa:** Por que escolheu essa abordagem?
Resposta: Simula eficientemente o ponteiro do relógio sem necessidade de listas encadeadas complexas, mantendo sincronia com a tabela de páginas

### 2.2 Organização do Código

Descreva como organizou seu código:

- Quantos arquivos/módulos criou?
  Resposta: Apenas um arquivo (simulador_memoria.c)
- Qual a responsabilidade de cada arquivo/módulo?
Resposta: Contém todas as definições de estruturas (Process, Frame, Queue), lógica de parsing dos arquivos de entrada, implementação dos algoritmos de substituição e o loop principal de execução
- Quais são as principais funções e o que cada uma faz?
Resposta: init_from_config: Lê o arquivo de configuração e aloca as estruturas iniciais. handle_access_fifo / handle_access_clock: Gerencia a lógica de hit/miss e orquestra a substituição para cada algoritmo. select_victim_fifo / select_victim_clock: Identifica qual frame será liberado. allocate_page_to_frame / deallocate_frame: Atualiza os metadados nos frames e nas tabelas de páginas dos processos.

**Exemplo:**
```
simulador.c
├── main() - lê argumentos e coordena execução
├── ler_config() - processa arquivo de configuração
├── processar_acessos() - loop principal de simulação
├── traduzir_endereco() - calcula página e deslocamento
├── consultar_tabela() - verifica se página está na memória
├── tratar_page_fault() - lida com page faults
├── algoritmo_fifo() - seleciona vítima usando FIFO
└── algoritmo_clock() - seleciona vítima usando Clock
```

### 2.3 Algoritmo FIFO

Explique **como** implementou a lógica FIFO:

- Como mantém o controle da ordem de chegada?
Resposta: Utiliza uma estrutura de Fila (Queue) onde os índices dos frames são inseridos no final (tail) assim que são preenchidos
- Como seleciona a página vítima?
Resposta: O algoritmo remove o índice que está no início (head) da fila, representando o frame que foi preenchido há mais tempo
- Quais passos executa ao substituir uma página?
Resposta: Remove o ID do frame vítima da fila. Desaloca a página antiga (atualiza tabela do processo antigo). Mapeia a nova página no frame liberado. Insere o ID deste frame novamente no final da fila (pois agora contém uma "nova" página).

**Não cole código aqui.** Explique a lógica em linguagem natural.

### 2.4 Algoritmo Clock

Explique **como** implementou a lógica Clock:

- Como gerencia o ponteiro circular?
Resposta: Uma variável inteira (clock_hand) serve como índice no array de frames. Ela é incrementada a cada verificação e usa operação de módulo (%) para voltar ao zero quando atinge o limite
- Como implementou a "segunda chance"?
Resposta: Ao verificar um frame apontado pelo clock_hand: se o bit R for 1, o algoritmo muda o R para 0 e avança o ponteiro (dando a chance). Se o bit R for 0, este frame é eleito a vítima
- Como trata o caso onde todas as páginas têm R=1?
Resposta: O ponteiro percorre toda a lista zerando os bits (transformando 1 em 0). Ao retornar ao primeiro frame (agora com R=0), ele é selecionado como vítima
- Como garante que o R-bit é setado em todo acesso?
Resposta: Em qualquer acesso (seja Hit ou após um Page Fault), a função de acesso define explicitamente referenced = 1 tanto na estrutura do Frame quanto na entrada da Tabela de Páginas do processo.

**Não cole código aqui.** Explique a lógica em linguagem natural.

### 2.5 Tratamento de Page Fault

Explique como seu código distingue e trata os dois cenários:

**Cenário 1: Frame livre disponível**
- Como identifica que há frame livre?
Resposta: A função find_free_frame percorre o array global de frames; se encontrar um com occupied == 0, retorna seu índice
- Quais passos executa para alocar a página?
Resposta: O código marca o frame como ocupado, registra o PID e página no frame, atualiza a tabela de páginas do processo (bit V=1) e, no caso do FIFO, adiciona o índice à fila

**Cenário 2: Memória cheia (substituição)**
- Como identifica que a memória está cheia?
Resposta: Quando a função find_free_frame retorna -1 após varrer todos os frames
- Como decide qual algoritmo usar (FIFO vs Clock)?
Resposta: A escolha do algoritmo é feita no início da main ao ler o argumento da linha de comando, chamando a função específica (handle_access_fifo ou handle_access_clock)
- Quais passos executa para substituir uma página?
Resposta: Seleção: Escolhe o frame vítima usando a lógica específica (remove da head da fila no FIFO ou varre ponteiro circular no Clock). Desalocação: Acessa o processo dono da página antiga e atualiza sua tabela de páginas (bit V=0, Frame=-1). Realocação: Sobrescreve os dados do frame com as informações da nova página e atualiza a tabela do novo processo (bit V=1).

---

## 3. Análise Comparativa FIFO vs Clock

### 3.1 Resultados dos Testes

Preencha a tabela abaixo com os resultados de pelo menos 3 testes diferentes:

| Descrição do Teste | Total de Acessos | Page Faults FIFO | Page Faults Clock | Diferença |
|-------------------|------------------|------------------|-------------------|-----------|
| Teste 1 - Básico  |        8          |            5      |           5        |       0    |
| Teste 2 - Memória Pequena |      10    |            10      |       10            |    0       |
| Teste 3 - Simples |        7          |          4        |              4     |     0      |
| Teste Próprio 1   |          8        |        5          |          5         |     0      |

### 3.2 Análise

Com base nos resultados acima, responda:

1. **Qual algoritmo teve melhor desempenho (menos page faults)?**
Resposta: Nos três testes, FIFO e Clock tiveram exatamente o mesmo número de page faults.
2. **Por que você acha que isso aconteceu?**
   Resposta:Isso acontece porque:

Quando não ocorre substituição, ambos são idênticos.

Quando ocorre substituição sem reutilização, Clock não consegue se beneficiar do R-bit.

A vantagem do Clock aparece apenas quando há localidade temporal verdadeira, e a memória é pequena demais para manter todas as páginas relevantes. Isso não ocorreu em nenhum dos seus testes.
 
4. **Em que situações Clock é melhor que FIFO?**
  Resposta: Clock supera FIFO quando:

O programa acessa páginas em loops.

O conjunto de páginas ativas é maior que o número de frames.

Existem páginas que são reutilizadas várias vezes.

5. **Houve casos onde FIFO e Clock tiveram o mesmo resultado?**
Resposta: Sim, todos os testes tiveram resultados iguais.

Isso aconteceu porque:

Ou não havia substituição,

Ou não havia localidade temporal,

Ou a memória comportava todas as páginas acessadas,

Ou o padrão de acesso anulava a vantagem do Clock.

6. **Qual algoritmo você escolheria para um sistema real e por quê?**

Resposta:Clock, porque:

É simples.

Evita substituir páginas recentemente usadas.

Se aproxima de LRU com baixo custo.

É amplamente usado em sistemas reais.

Mesmo quando empata com FIFO, Clock nunca é pior, mas pode ser me

## 4. Desafios e Aprendizados

### 4.1 Maior Desafio Técnico

Descreva o maior desafio técnico que seu grupo enfrentou durante a implementação:

Resposta:

Qual foi o problema?

A parte mais complicada foi implementar:

as tabelas de páginas,

o vetor de frames,

e a lógica completa do algoritmo Clock
(ponteiro circular + bit R).

Como identificaram o problema?

Durante os testes iniciais, percebemos que:

páginas eram substituídas na hora errada,

o ponteiro não avançava corretamente,

o bit R não estava sendo resetado na hora certa.

Como resolveram?

Revisamos o algoritmo original:

Passar pelo vetor de frames em formato circular

Verificar R

Dar segunda chance se R=1

Substituir apenas quando encontrar R=0

Garantir wrap-around do ponteiro ((ptr+1) % num_frames)

Testamos passo a passo com os arquivos pequenos do professor até o comportamento ficar correto.

O que aprenderam?

A importância de validar o algoritmo com casos simples antes dos grandes

A dificuldade real de sincronizar ponteiros, bits e tabelas

Que debugging em sistemas de memória exige observar o estado interno de cada frame

### 4.2 Principal Aprendizado

Descreva o principal aprendizado sobre gerenciamento de memória que vocês tiveram com este projeto:

Resposta:

O que vocês não entendiam bem antes e agora entendem?

Antes era difícil visualizar:

como uma página entra na memória,

como ocorre a substituição,

e por que certos algoritmos são melhores que outros.

Agora, ficou claro como:

O bit R influencia a decisão

FIFO pode tomar decisões ruins

Clock imita LRU de maneira eficiente

O conceito de localidade afeta fortemente o desempenho

Como este projeto mudou a compreensão de vocês sobre memória virtual?

Agora entendemos que memória virtual não é só “um conceito teórico” —
é um mecanismo complexo que depende de:

estruturas de dados

políticas de substituição

padrões de acesso do programa

otimização de hardware (MMU, TLB, bits R e M)

---

## 5. Vídeo de Demonstração

**Link do vídeo:** [Insira aqui o link para YouTube, Google Drive, etc.]

### Conteúdo do vídeo:

Confirme que o vídeo contém:

- [ ] Demonstração da compilação do projeto
- [ ] Execução do simulador com algoritmo FIFO
- [ ] Execução do simulador com algoritmo Clock
- [ ] Explicação da saída produzida
- [ ] Comparação dos resultados FIFO vs Clock
- [ ] Breve explicação de uma decisão de design importante

---

## Checklist de Entrega

Antes de submeter, verifique:

- [ ] Código compila sem erros conforme instruções da seção 1.1
- [ ] Simulador funciona corretamente com FIFO
- [ ] Simulador funciona corretamente com Clock
- [ ] Formato de saída segue EXATAMENTE a especificação do ENUNCIADO.md
- [ ] Testamos com os casos fornecidos em tests/
- [ ] Todas as seções deste relatório foram preenchidas
- [ ] Análise comparativa foi realizada com dados reais
- [ ] Vídeo de demonstração foi gravado e link está funcionando
- [ ] Todos os integrantes participaram e concordam com a submissão

---
## Referências
Liste aqui quaisquer referências que utilizaram para auxiliar na implementação (livros, artigos, sites, **links para conversas com IAs.**)

Aulas 9 para frente em especifico as aulas 11 12 e 13 que nos ajudaram mais https://graduacao.mackenzie.br/course/view.php?id=27998

Livro da ufrgs que nos ajudou a entender melhor este topico:
http://www.inf.ufrgs.br/~asc/livro/secao65.pdf

Biblioteca dev no github que tambem nos ajudou:
https://github.com/KAYOKG/BibliotecaDev

---
