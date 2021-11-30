#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>

#define ENDERECO_CON "127.0.0.1" //192.168.1.1
#define PORTA_CON 3001

int     g_socket[4], g_tempoMedicao=0; // g_socket[0] = socket do servidor.
float   g_temperatura=0, g_ultimasTemperaturas[10];
bool    g_limiteInferior=0, g_limiteSuperior=0;

struct sockaddr_in servAddr, cliAddr;
socklen_t   cliAddr_size = (socklen_t)sizeof(cliAddr);
pthread_t   pthread_tarefa[4];
sem_t       sem_tarefa[4];

typedef struct {
    float   temperatura;
    bool    limiteSuperior;
    bool    limiteInferior;
    int     tempoMedicao;
} Sensor;

void ConectarSensorTemperatura()
{
    printf("Aguardando sensores de Temperatura...\n");
    g_socket[1] = accept(g_socket[0], (struct sockaddr *)&cliAddr, &cliAddr_size);
}

void ConectarSensorLimiteSuperior()
{
    printf("Aguardando sensores do Limite Superior...\n");
    g_socket[2] = accept(g_socket[0], (struct sockaddr *)&cliAddr, &cliAddr_size);
}

void ConectarSensorLimiteInferior()
{
    printf("Aguardando sensores do Limite Inferior...\n\n");
    g_socket[3] = accept(g_socket[0], (struct sockaddr *)&cliAddr, &cliAddr_size);
}

void InicializarSemaforos()
{
    printf("Inicializando semáforos..\n");
    for (int i = 0; i < 4; i++)
        sem_init(&sem_tarefa[i], 0, 1);
    printf("Semáforos inicializados com sucesso..\n");
}

int Conectar()
{
    g_socket[0] = socket(AF_INET, SOCK_STREAM, 0);
    if (g_socket[0] < 0)
        perror("Erro ao inicializar socket do servidor.\n");

    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(PORTA_CON);
    inet_aton(ENDERECO_CON, &(servAddr.sin_addr));

    printf("Tentando abrir porta %d...\n", PORTA_CON);
    if (bind(g_socket[0], (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
    {
        printf("Problemas ao abrir porta...\n");
        printf("Execucao finalizada!\n");
        return 0;
    }
    printf("Porta %d aberta!\n", PORTA_CON);
    listen(g_socket[0], 10);

    InicializarSemaforos();
    ConectarSensorTemperatura();
    ConectarSensorLimiteSuperior();
    ConectarSensorLimiteInferior();
}

float ObterMediaTemperaturas()
{
    float somaTemperaturas = 0;
    for (int i = 0; i < 10; i++)
        somaTemperaturas += g_ultimasTemperaturas[i];
    return somaTemperaturas / 10;
}

void *TP0_TemperaturaReservatorio()
{
    int indiceVetorTemperaturas = 0;
    
    while (1)
    {
        sem_wait(&sem_tarefa[0]);
        Sensor sensor;

        recv(g_socket[1], &sensor, sizeof(Sensor), 0);
        g_temperatura = sensor.temperatura;
        g_tempoMedicao = sensor.tempoMedicao;
        g_ultimasTemperaturas[indiceVetorTemperaturas] = sensor.temperatura;
        indiceVetorTemperaturas++;
        if(indiceVetorTemperaturas > 9) indiceVetorTemperaturas = 0;
        
        printf("(T=%ds) Temperatura = %.1f °C\n", sensor.tempoMedicao, sensor.temperatura);
        sleep(1);
        sem_post(&sem_tarefa[0]);
    }

    close(g_socket[1]);
}

void *TP1_LimiteSuperiorReservatorio()
{
    while (1)
    {
        sem_wait(&sem_tarefa[1]);
        Sensor sensor;
        sleep(2);
        recv(g_socket[2], &sensor, sizeof(Sensor), 0);
        g_limiteSuperior = sensor.limiteSuperior;
        printf("(T=%ds) Limite Superior = %d\n", sensor.tempoMedicao, sensor.limiteSuperior);
        sem_post(&sem_tarefa[1]);
    }

    close(g_socket[2]);
}

void *TP2_LimiteInferiorReservatorio()
{
    while (1)
    {
        Sensor sensor;
        sem_wait(&sem_tarefa[2]);
        sleep(3);
        recv(g_socket[3], &sensor, sizeof(Sensor), 0);
        g_limiteInferior = sensor.limiteInferior;
        printf("(T=%ds) Limite Inferior = %d\n", sensor.tempoMedicao, sensor.limiteInferior);
        sem_post(&sem_tarefa[2]);
    }

    close(g_socket[3]);
}

void *TA_VerificadorDados()
{
    
    while (1)
    {
        Sensor sensor;
        sem_wait(&sem_tarefa[3]);
        float tempoParaProximaExecucao = rand() % 6; // Randômico entre 0 e 6.
        sleep(tempoParaProximaExecucao);
        printf("(T=%ds) VERIFICANDO LIMITES...\n", g_tempoMedicao);
        float mediaTemperaturas = ObterMediaTemperaturas();       

        if (mediaTemperaturas > 70)
            printf("(T=%ds) ALERTA DE SUPERAQUECIMENTO -> TEMPERATURA MÉDIA %.1f °C\n", g_tempoMedicao, mediaTemperaturas);

        if (g_limiteInferior == 1)
            printf("(T=%ds) AVISO: O limite INFERIOR do reservatório não está sendo respeitado!\n", g_tempoMedicao);

        if (g_limiteSuperior == 1)
            printf("(T=%ds) AVISO: O limite SUPERIOR do reservatório não está sendo respeitado!\n", g_tempoMedicao);

        sleep(6 - tempoParaProximaExecucao);  // Complemento do período de 6 segundos.
        sem_post(&sem_tarefa[3]);
    }
}

int main()
{
    Conectar();

    pthread_create(&pthread_tarefa[0], NULL, (void *)TP0_TemperaturaReservatorio, NULL);
    pthread_create(&pthread_tarefa[1], NULL, (void *)TP1_LimiteSuperiorReservatorio, NULL);
    pthread_create(&pthread_tarefa[2], NULL, (void *)TP2_LimiteInferiorReservatorio, NULL);
    pthread_create(&pthread_tarefa[3], NULL, (void *)TA_VerificadorDados, NULL);

    for (int i = 0; i < 4; i++)
        pthread_join(pthread_tarefa[i], NULL);

    return 0;
}