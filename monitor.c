// Estas são as bibliotecas utilizadas para a funcionalidade do nosso monitor de rede:

#include <stdio.h>       // Biblioteca de entrada e saída (ex.: printf), amplamente utilizada em diferentes aplicações
#include <string.h>      // Biblioteca para manipulação de strings, oferecendo funções de busca, cópia e formatação
#include <arpa/inet.h>   // Biblioteca usada para conversão de endereços de rede do binário para legível
#include <ifaddrs.h>     // Permite coletar informações sobre todas as interfaces de rede disponíveis no sistema

// Variável global que separa visualmente os endereços IPv4 e IPv6 na impressão, acrescentando uma linha em branco
int ipv6_ja_impresso = 0;

// Função para listar todas as interfaces de rede e seus endereços IPv4/IPv6
void listar_enderecos_ip() {
    struct ifaddrs *ifap, *ifa;    // Ponteiros usados para percorrer a lista de interfaces de rede
    char addr[INET6_ADDRSTRLEN];  // Espaço para armazenar o endereço IP convertido do binário para texto (IPv4 ou IPv6)

    // Estrutura para obter a lista de interfaces de rede do sistema
    if (getifaddrs(&ifap) == -1) {
        perror("Erro ao coletar as informações");  // Mensagem de erro, caso a coleta falhe por algum motivo
        return;
    }

    // Percorre a lista ligada de interfaces, com a utilização de um "for loop", excluindo ponteiros nulos
    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr) {
            // Verifica se o endereço é IPv4
            if (ifa->ifa_addr->sa_family == AF_INET) {
                struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
                inet_ntop(AF_INET, &sa->sin_addr, addr, sizeof(addr));
                printf("Interface: %s\tEndereço IPv4: %s\n", ifa->ifa_name, addr);
            }
            // Verifica se o endereço é IPv6
            else if (ifa->ifa_addr->sa_family == AF_INET6) {
                // Aqui é usada a variável (também chamada, neste contexto, de "flag") para acrescentar uma linha nova, sem conflito com o "for"
                if (!ipv6_ja_impresso) {
                    printf("\n");
                    ipv6_ja_impresso = 1;
                } // Fim da estrutura da flag de nova linha, e continuação do algoritmo de identificação IPv6
                struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)ifa->ifa_addr;
                inet_ntop(AF_INET6, &sa6->sin6_addr, addr, sizeof(addr));
                printf("Interface: %s\tEndereço IPv6: %s\n", ifa->ifa_name, addr);
            }
        }
    }
    // Libera a memória alocada por getifaddrs, que não é feito de forma automática
    freeifaddrs(ifap);
}

// A implementação do comando ping pode ser feita diretamente com sockets ICMP (exige root) ou chamando o comando "ping" via popen().
// Optamos por popen() pela simplicidade e por não necessitar privilégios de administrador.
int ping_via_popen(const char *host, int count, double *out_avg, double *out_loss) {
    char comando[256]; // Variável para armazenar o comando ping que será executado
    char linha[512]; // Buffer para ler cada linha de saída do ping
    FILE *canal; // Ponteiro para o pipe que conecta o programa ao comando do sistema
    int perda_identificada = 0; // Variável (ou flag) para indicar se a perda de pacotes foi identificada
    int rtt_identificado = 0; // Flag para indicar se o RTT (tempo de ida e volta de um pacote entre origem e destino) médio foi identificado

    // Monta o comando ping que será executado pelo sistema
    // "-c %d" define o número de pacotes a enviar, e "2>&1" garante que mensagens de erro também sejam capturadas,
    // em conjunto com a saída normal.
    snprintf(comando, sizeof(comando), "ping -c %d %s 2>&1", count, host);

    canal = popen(comando, "r");
    if (!canal) {
        perror("popen falhou");
        return -1; // Algoritmo simples para identificar erro ao abrir o pipe
        // É boa prática retornar um valor diferente de 0 caso ocorra algum erro
    }

    // Lê cada linha da saída do ping e procura por informações úteis:
    // - Resumo de transmissão que contéma  taxa de perda (packet loss)
    // - Linha de estatísticas que contém o RTT
    while (fgets(linha, sizeof(linha), canal) != NULL) {
        // Procura a linha com "packet loss" (ou porcentagem de perda de pacotes)
        // A busca ocorre em dois formatos comuns do utilitário ping, para garantir a coleta das informações
        if (strstr(linha, "packet loss") != NULL) {
            int tx = 0, rx = 0;
            double perda = 0.0; // Variável que representa a taxa de perda em porcentagem
            // Formato comum ("X packets transmitted, Y received, Z packet loss, time N ms"):
            if (sscanf(linha, "%d packets transmitted, %d received, %lf%% packet loss", &tx, &rx, &perda) >= 3) {
                if (out_loss) *out_loss = perda;
                perda_identificada = 1;
            }
            // Formato alternativo (algumas versões do ping apresentam "X packets transmitted, Y packets received, ...")
            else {
                if (sscanf(linha, "%d packets transmitted, %d packets received, %lf%% packet loss", &tx, &rx, &perda) >= 3) {
                    if (out_loss) *out_loss = perda;
                    perda_identificada = 1;
                }
            }
        }

        // Detecta a linha de estatísticas RTT no formato Linux:
        // exemplo: "rtt min/avg/max/mdev = 10.123/11.234/12.345/0.456 ms"
        // Localizamos '=' e lemos os valores separados por '/' para extrair o avg (média de RTT).
        if (strstr(linha, "rtt ") != NULL || strstr(linha, "round-trip") != NULL) {
            char *eq = strchr(linha, '=');
            if (eq) {
                double v1, v2, v3, v4;
                if (sscanf(eq + 1, " %lf/%lf/%lf/%lf", &v1, &v2, &v3, &v4) >= 2) {
                    // v2 geralmente é avg
                    if (out_avg) *out_avg = v2;
                    rtt_identificado = 1;
                } else {
                    // Algumas variantes podem ter "min/avg/max" (3 valores, comparado aos 4 do formato acima)
                    if (sscanf(eq + 1, " %lf/%lf/%lf", &v1, &v2, &v3) == 3) {
                        if (out_avg) *out_avg = v2;
                        rtt_identificado = 1;
                    }
                }
            }
        }
    }

    // Fecha o pipe e obtém a saída do comando (é importante fechar o pipe para que não ocorram vazamentos de memória)
    int status = pclose(canal);

    // Se não detectamos nem perda nem RTT, sinalizamos erro.
    if (!perda_identificada && !rtt_identificado) {
        return -1;
    }
    return 0;
}

// Função responsável por medir RTT (latência média) até um host.
// Ela utiliza ping_via_popen() para executar o ping e extrair a informação desejada.
double medir_latencia(const char *host, int pacotes) {
    double rtt_medio = -1.0, loss = 0.0; // Variáveis para armazenar o RTT médio e a taxa de perda de pacotes
    int res = ping_via_popen(host, pacotes, &rtt_medio, &loss); // Chama a função de ping e coleta os dados
    if (res != 0) {
        printf("Não foi possível obter latência via ping para %s\n", host);
        return -1.0;
    }
    // Se o ping foi executado, mas a saída não continha a linha de RTT, avisamos o usuário:
    if (rtt_medio < 0) {
        printf("Ping executado, mas RTT médio não foi encontrado na saída para %s\n", host);
    }
    return rtt_medio; // Retorna o RTT médio (ou -1, em caso de falha)
}

// Como o nome sugere, trata-se de uma função para calcular a perda de pacotes. Faz uso da função ping_via_popen() em sua lógica
double calcular_perda_pacotes(const char *host, int pacotes) {
    double avg = -1.0, loss = -1.0; // Variáveis utilizadas para armazenar RTT médio e taxa de perda de pacotes
    int res = ping_via_popen(host, pacotes, &avg, &loss); // Executa o ping e coleta resultados
    if (res != 0) {
        printf("Não foi possível obter perda de pacotes via ping para %s\n", host);
        return -1.0;
    }
    return loss; // Retorna taxa de perda em porcentagem
}

/* ------------------------------------------------------------------ */
/* Aqui se inicia a função main(), que é onde o código realmente é executado, após todas as bibliotecas, variáveis
/* e demais funções já estarem declaradas. Buscamos manter o main() simples e legível, para fácil compreensão do código.
/* ------------------------------------------------------------------ */
int main() {
    printf("Estas são as informações do monitor de rede do grupo '6':\n\n");

    // Chama a função de listar endereços IPv4 e IPv6, apresentada anteriormente no código
    listar_enderecos_ip();

    // Leitura do IP alvo fornecido pelo usuário (scanf), além do tratamento de erros
    char alvo[64];
    int pacotes = 4;
    printf("\nInforme o endereço IP alvo (ex: 8.8.8.8): ");
    if (scanf("%63s", alvo) != 1) {
        fprintf(stderr, "Entrada inválida.\n");
        return 1;
    }

    // Mede latência média (RTT) em ms para o IP fornecido
    double rtt_medio = medir_latencia(alvo, pacotes);
    if (rtt_medio >= 0)
        printf("\nRTT médio para %s = %.3f ms\n", alvo, rtt_medio);

    // Calcula taxa de perda de pacotes (em %) para o mesmo IP
    double perda = calcular_perda_pacotes(alvo, pacotes);
    if (perda >= 0)
        printf("Taxa de perda para %s = %.2f%%\n", alvo, perda);
    printf("\n------------------------------------------------------------------\n");
    printf("Agradecemos por usar o nosso monitor de rede! :)\n");
    printf("\nDesenvolvido por: \nBruno Espinosa Pavulack, \nGuilherme Luerce Neves, \nMauricio Lichtnow Ott, \nPaulo César Rodrigues Masson, \nWillian gonçalves Gomes\n \n");
    return 0;
}
