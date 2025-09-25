// Estas são as bibliotecas utilizadas para a funcionalidade do nosso monitor de rede:

#include <stdio.h>       // Biblioteca de entrada e saída (ex.: printf), amplamente utilizada em diferentes aplicações
#include <stdlib.h>      // Biblioteca com funções utilitárias como alocação de memória e execução de comandos externos
#include <string.h>      // Biblioteca para manipulação de strings, oferecendo funções de busca, cópia e formatação
#include <sys/socket.h>  // Define estruturas e funções para criação e uso de sockets (pontos de comunicação em rede)
#include <arpa/inet.h>   // Biblioteca usada para conversão de endereços de rede entre formatos humano e binário
#include <ifaddrs.h>     // Permite coletar informações sobre todas as interfaces de rede disponíveis no sistema

int ipv6_printed = 0; // Marcador futuramente usado para separar os endereços IPv4 e IPv6 com uma linha em branco

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
                if (!ipv6_printed) {
                    printf("\n");
                    ipv6_printed = 1;
                }
                struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)ifa->ifa_addr;
                inet_ntop(AF_INET6, &sa6->sin6_addr, addr, sizeof(addr));
                printf("Interface: %s\tEndereço IPv6: %s\n", ifa->ifa_name, addr);
            }
        }
    }
    // Libera a memória alocada por getifaddrs
    freeifaddrs(ifap);
}

int main() {
    // Chama a função que lista os endereços IPv4 das interfaces
    listar_enderecos_ip();
    return 0;
}
