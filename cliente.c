#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <stdbool.h>

#define ENDERECO_CON "127.0.0.1"
#define PORTA_CON 3001
#define VOLUME_MAXIMO 70.0
#define VOLUME_MINIMO 20.0
#define ENCHER_RESERVATORIO true

int g_socket[3], g_tempoMedicao = 0;
float g_volumeReservatorio;

struct sockaddr_in servAddr;
pthread_t   pthread_tarefa[5];
sem_t       sem_tarefa[5];

typedef struct {
    float   temperatura;
    bool    limiteSuperior;
    bool    limiteInferior;
    int     tempoMedicao;
} Sensor;

int CriarSocket(int id)
{
    int socktTemp = socket(AF_INET, SOCK_STREAM, 0);
    if(socktTemp < 0) {
        printf("Erro ao inicializar socket.");
        exit(1);
    } else {
        printf("Socket %d criado...\n", id);
    }
    return socktTemp;
}

void ConectarSocket(int _socket, int id)
{
    if (connect(_socket, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
    {
        printf("Problemas na conexão...\n");
        printf("Execução abortada com sucesso!\n");
        exit(1);
    }
    printf("Socket %d conectado!\n", id);
}

int Conectar()
{    
    printf("Inicializando Sockets de Comunicação...\n");
    g_socket[0] = CriarSocket(0);
    g_socket[1] = CriarSocket(1);
    g_socket[2] = CriarSocket(2);
    printf("Sockets criados com sucesso...\n");

    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(PORTA_CON);
    inet_aton(ENDERECO_CON, &(servAddr.sin_addr));

    printf("Tentando conexão com servidor...\n");
    ConectarSocket(g_socket[0], 0);
    ConectarSocket(g_socket[1], 1);
    ConectarSocket(g_socket[2], 2);
    printf("Todos sockets conectados ao servidor...\n");

    printf("Conexão estabelecida com sucesso!\n\n");
    srand(time(NULL));
}

void *AtualizarTempoMedicao()
{
    while (1)
    {
        sem_wait(&sem_tarefa[0]);
        sleep(1);
        g_tempoMedicao++;
        sem_post(&sem_tarefa[0]);
    }
}

void *ControlarVazao()
{
    // Volume inicial
    if (ENCHER_RESERVATORIO) g_volumeReservatorio = 25.0;
    else g_volumeReservatorio = 65.0;

    while (1)
    {
        sem_wait(&sem_tarefa[1]);
        if(ENCHER_RESERVATORIO) g_volumeReservatorio += 4.5;
        else g_volumeReservatorio -= 4.5;
        sleep(1);
        sem_post(&sem_tarefa[1]);
    }
}

void *SensorTemperatura()
{
    Sensor sensor;
    int    periodo = 1;
    float  temperatura = 35;
    float  acrescTemperatura = 2.5;

    while (1)
    {
        sem_wait(&sem_tarefa[2]);
        sensor.temperatura = temperatura;
        sensor.tempoMedicao = g_tempoMedicao;
        temperatura += acrescTemperatura;
        send(g_socket[0], &sensor, sizeof(Sensor), 0);
        printf("(T=%ds) Enviando indicador de Temperatura...    (T=%.1f °C)\n", g_tempoMedicao, sensor.temperatura);
        sleep(periodo);
        sem_post(&sem_tarefa[2]);
    }

    close(g_socket[0]);
}

void *SensorLimiteSuperior()
{
    Sensor sensor;
    int    periodo = 2;

    while (1)
    {
        sem_wait(&sem_tarefa[3]);
        sleep(periodo);
        sensor.limiteSuperior = false;
        sensor.tempoMedicao = g_tempoMedicao;
        if (g_volumeReservatorio > VOLUME_MAXIMO) sensor.limiteSuperior = true;
        printf("(T=%ds) Enviando indicador do Limite Superior...  (Vol=%.1f, LimSup=%d)\n", g_tempoMedicao, g_volumeReservatorio, sensor.limiteSuperior);
        send(g_socket[1], &sensor, sizeof(Sensor), 0);
        sem_post(&sem_tarefa[3]);
    }

    close(g_socket[1]);
}

void *SensorLimiteInferior()
{
    Sensor sensor;
    int periodo = 3;

    while (1)
    {
        sem_wait(&sem_tarefa[4]);
        sleep(periodo);
        sensor.limiteInferior = false;
        sensor.tempoMedicao = g_tempoMedicao;
        if (g_volumeReservatorio < VOLUME_MINIMO) sensor.limiteInferior = true;
        printf("(T=%ds) Enviando indicador do Limite Inferior...  (V=%.1f, LimInf=%d)\n", g_tempoMedicao, g_volumeReservatorio, sensor.limiteInferior);
        send(g_socket[2], &sensor, sizeof(Sensor), 0);
        sem_post(&sem_tarefa[4]);
    }

    close(g_socket[2]);
}

int main()
{
    Conectar();

    for (size_t i = 0; i < 5; i++)
        sem_init(&sem_tarefa[i], 0, 1);

    pthread_create(&pthread_tarefa[0], NULL, (void *)ControlarVazao, NULL);
    pthread_create(&pthread_tarefa[1], NULL, (void *)AtualizarTempoMedicao, NULL);
    pthread_create(&pthread_tarefa[2], NULL, (void *)SensorTemperatura, NULL);
    pthread_create(&pthread_tarefa[3], NULL, (void *)SensorLimiteSuperior, NULL);
    pthread_create(&pthread_tarefa[4], NULL, (void *)SensorLimiteInferior, NULL);

    for (size_t i = 0; i < 5; i++)
        pthread_join(pthread_tarefa[i], NULL);
    
    return 0;
}