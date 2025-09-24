#include <arpa/inet.h>   // Funções de conversão de endereços (inet_ntoa, inet_pton, etc.)
#include <sys/socket.h>  // Definições de estruturas de socket e famílias de endereços
#include <ifaddrs.h>     // Função getifaddrs e estrutura ifaddrs (interfaces de rede)
#include <stdio.h>       // Entrada e saída padrão (printf)

// Função para listar todas as interfaces de rede e seus endereços IPv4
void listar_enderecos_ipv4() {
    struct ifaddrs *ifap, *ifa;       // Ponteiros para lista encadeada de interfaces
    struct sockaddr_in *sa;           // Estrutura para armazenar endereço IPv4
    char *addr;                       // String com o endereço convertido

    // Obtém a lista de interfaces de rede do sistema
    if (getifaddrs(&ifap) == -1) {
        perror("Erro ao obter interfaces");  // Mensagem de erro se falhar
        return;
    }

    // Percorre a lista ligada de interfaces
    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        // Confere se a interface tem endereço e se é da família IPv4
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            sa = (struct sockaddr_in *) ifa->ifa_addr;   // Converte para sockaddr_in
            addr = inet_ntoa(sa->sin_addr);              // Converte endereço binário para string legível

            // Exibe o nome da interface (ex: eth0, wlan0, lo) e seu IP
            printf("Interface: %s\tEndereço IPv4: %s\n", ifa->ifa_name, addr);
        }
    }

    // Libera a memória alocada por getifaddrs
    freeifaddrs(ifap);
}

int main() {
    // Chama a função que lista os endereços IPv4 das interfaces
    listar_enderecos_ipv4();
    return 0;
}
