#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>


// #define PORT 8080
#define BUFFER_SIZE 2048
#define MAX_ROOMS 5
#define MAX_PLAYERS_PER_ROOM 2
#define MAX_SAVED_PLAYERS 20

typedef enum {
    CHOOSING_ROOM,
    IN_LOBBY,
    IN_GAME,
    GAME_OVER,
    NICKNAME,
} Status;

typedef struct {
    int socket;
    char nickname[50];
    bool is_connected;
    bool in_game;
    int secret;
    bool ready_to_start; // Флаг готовности игрока
    bool is_turn;
    time_t disconnect_time; // Последнее время получения пинга
    time_t last_ping_time; // Последнее время получения пинга
    bool pending_reconnect; // Флаг для обозначения временно отключенного игрока
    Status status;
    int bad_input_count;
} Player;

typedef struct {
    Player players[MAX_PLAYERS_PER_ROOM];
    int player_count;
    bool game_started;
    char room_name[50];
    char saved_nicknames[MAX_PLAYERS_PER_ROOM][50]; // Список сохраненных игроков
    int saved_sockets[2];
    int saved_player_count; // Количество сохраненных игроков
} Room;

typedef struct {
    const char *name;                                // Имя команды
    void (*handler)(int socket, const char *args, char *nickname); // Функция-обработчик
    bool requires_args;                              // Требует ли команда аргументы
} Command;

typedef struct {
    int socket;
    char nickname[50];
    time_t last_ping_time;
    bool is_connected;
    int bad_command_count; // Количество неправильных команд подряд
} ClientInfo;

ClientInfo clients[MAX_SAVED_PLAYERS]; // Массив информации о клиентах
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void handle_leave_game(int socket, const char *args, char *nickname);
void handle_create_lobby(int socket, const char *args, char *nickname);
void handle_user_secret(int socket, const char *args, char *nickname);
bool handle_nickname(int socket, const char *args, char *nickname);
void handle_ping(int socket, const char *args, char *nickname);
void handle_join_room(int socket, const char *args, char *nickname);
void handle_leave_room(int socket, const char *args, char *nickname);
void handle_get_player_count(int socket, const char *args, char *nickname);
void handle_get_rooms(int socket, const char *args, char *nickname);
void handle_start_game(int socket, const char *args, char *nickname);
void handle_guess(int socket, const char *args, char *nickname);


Command command_list[] = {
    {"LEAVE_GAME", handle_leave_game, false},
    {"CREATE_LOBBY", handle_create_lobby, false},
    {"USER_SECRET:", handle_user_secret, true},
    // {"NICKNAME:", handle_nickname, true},
    {"PING", handle_ping, false},
    {"JOIN_ROOM:", handle_join_room, true},
    {"LEAVE_ROOM", handle_leave_room, false},
    {"GET_PLAYER_COUNT:", handle_get_player_count, true},
    {"GET_ROOMS", handle_get_rooms, false},
    {"START_GAME", handle_start_game, false},
    {"GUESS:", handle_guess, true},
};

typedef struct {
    char key[50];
    char value[200];
} Message;

Message messages[] = {
    {"connection_error", "con_error, recon plz"},
    {"game_start", "The game has started!"},
    {"player_joined", "A new player has joined the room."},
    {"player_left", "A player has left the room."}
};

const int command_count = sizeof(command_list) / sizeof(command_list[0]);

Room rooms[MAX_ROOMS];
int room_count = 0;
bool gameStartedFull = false;
bool gameStartedEmpty = false;
int saved_player_count = 0;
int server_fd;


pthread_mutex_t room_mutex = PTHREAD_MUTEX_INITIALIZER;

void send_message(int client_socket, const char *message);
void send_room_list(int client_socket, const char* nickname);
void create_room(int client_socket, const char* nickname);
void join_room(int client_socket, const char* room_name, const char* nickname);
void send_player_count(int client_socket, const char* room_name);
void* handle_client(void* client_socket);
void leave_room(int socket);
bool calculate_bulls_and_cows(const char * str, const char * text, int * bulls, int * cows);
void* check_player_pings();
void leave_game(int client_socket);
int save_player_to_room(Room* room, const char* nickname);
int check_save_status(Room * room, const char * nickname);
bool isAsciiString(const char *str);
bool isValidGuess(const char *guess);
Player* find_player(const char* nickname);
ClientInfo* find_player_socket(int socket);
bool is_nickname_taken(const char* nickname);
int testClinetInfo(const char* nickname);
void close_server();

void load_config(const char *filename, char *ip, int *port) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open config file");
        exit(EXIT_FAILURE);
    }

    if (fscanf(file, "%15s %d", ip, port) != 2) {
        fprintf(stderr, "Invalid config file format\n");
        exit(EXIT_FAILURE);
    }

    fclose(file);
}

int main() {
    int new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char ip[16];
    int port;

    signal(SIGINT, close_server);

    // Загружаем конфигурацию из файла
    load_config("server_config.txt", ip, &port);
    printf("Server starting on IP: %s, Port: %d\n", ip, port);

    // Создаем сокет
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }
//123
    // int opt = 1;
    // if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    //     perror("setsockopt failed");
    //     close(server_fd);
    //     exit(EXIT_FAILURE);
    // }
//123
    // Настраиваем параметры адреса
    address.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip, &address.sin_addr) <= 0) {
        perror("Invalid IP address");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    address.sin_port = htons(port);

    // Привязываем сокет к адресу и порту
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Запускаем прослушивание входящих подключений
    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    pthread_t ping_thread;
    if (pthread_create(&ping_thread, NULL, check_player_pings, NULL) != 0) {
        perror("Failed to create ping thread");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on %s:%d\n", ip, port);

    // Основной цикл обработки подключений
    while (1) {
        printf("Waiting for a player to connect...\n");
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (new_socket < 0) {
            perror("Accept failed");
            close(server_fd);
            exit(EXIT_FAILURE);
        }

        printf("Player connected.\n");
        send_message(new_socket, "CON_OK");

        pthread_t client_thread;
        int *client_socket = malloc(sizeof(int));
        *client_socket = new_socket;
        if (pthread_create(&client_thread, NULL, handle_client, client_socket) != 0) {
            perror("Could not create thread");
            close(new_socket);
            free(client_socket);
        }else {
            printf("\n\n1\n\n");
            pthread_detach(client_thread);//123
        }
    }

    close(server_fd);
    return 0;
}

void* handle_client(void* client_socket) {
    int socket = *(int*)client_socket;
    free(client_socket);
    char buffer[BUFFER_SIZE] = {0};
    int valread;
    char nickname[50] = {0};

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_SAVED_PLAYERS; i++) {
        if (!clients[i].is_connected) {
            clients[i].socket = socket;
            clients[i].is_connected = true;
            clients[i].last_ping_time = time(NULL);
            strncpy(clients[i].nickname, nickname, sizeof(clients[i].nickname) - 1);
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    bool is_nick_set = false; //123
    while (strlen(nickname) == 0 || is_nick_set == false) {
        valread = read(socket, buffer, BUFFER_SIZE - 1);
        if (valread > 0) {
            buffer[valread] = '\0'; // Завершаем строку
            buffer[strcspn(buffer, "\r\n")] = '\0'; // Убираем \r\n

            if (!isAsciiString(buffer)) {
                send_message(socket, "ERROR:invalid input.\n");
                // printf("Received invalid data: %s\n", buffer);
                // close(socket);

                // return NULL;
                // pthread_mutex_lock(&clients_mutex);
                for (int i = 0; i < MAX_SAVED_PLAYERS; i++) {
                    if (clients[i].socket == socket) {
                        clients[i].bad_command_count++; // Увеличиваем счетчик
                        if (clients[i].bad_command_count >= 5) {
                            printf("DISCONNECTED: Too many invalid commands.");
                            send_message(socket, "DISCONNECTED: Too many invalid commands.");
                            clients[i].is_connected = false;

                            close(socket); // Закрываем соединение
                            pthread_exit(NULL);
                            break;
                        }
                    }
                }
                // pthread_mutex_unlock(&clients_mutex);
                continue;
            }

            if (strncmp(buffer, "NICKNAME:", 9) == 0) {
                strncpy(nickname, buffer + 9, sizeof(nickname) - 1);
                printf("RECV_NICK: %s\n", nickname);
                is_nick_set = handle_nickname(socket,0 ,nickname); //123
            } else {
                send_message(socket, "WARN:USE_NICK");
            }
        }else if(valread == 0){
            printf("Client disconnected before setting nickname.\n");
            close(socket);
            pthread_exit(NULL);
        }else {
            usleep(100);
        }
        //123
        // else {
        //     printf("\n\nFIND ME\n\n");
        // }
        //123
    }

    while ((valread = read(socket, buffer, BUFFER_SIZE - 1)) > 0) {
        buffer[valread] = '\0'; // Завершаем строку
        buffer[strcspn(buffer, "\r\n")] = '\0'; // Убираем \r\n

        if(!isAsciiString(buffer)) {
            send_message(socket, "ERROR:invalid input.\n");
            printf("Received invalid data: %s\n", buffer);
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < MAX_SAVED_PLAYERS; i++) {
                if (clients[i].socket == socket) {
                    clients[i].bad_command_count++; // Увеличиваем счетчик
                    if (clients[i].bad_command_count >= 5) {
                        printf("DISCONNECTED: Too many invalid commands.");
                        send_message(socket, "DISCONNECTED: Too many invalid commands.");
                        clients[i].is_connected = false;
                        close(socket); // Закрываем соединение
                        break;
                    }
                }
            }
            pthread_mutex_unlock(&clients_mutex);
        }

        ClientInfo *tmp_client = find_player_socket(socket);
        if(tmp_client == NULL) {
            return 0;
        }

        printf("Received from client: %s\n", buffer);

        // Найти подходящую команду
        bool command_found = false;
        for (int i = 0; i < command_count; i++) {
            if (strncmp(buffer, command_list[i].name, strlen(command_list[i].name)) == 0) {
                command_found = true;

                // Если команда требует аргументы
                const char *args = buffer + strlen(command_list[i].name);
                if (command_list[i].requires_args && (args == NULL || *args == '\0')) {
                    send_message(socket, "Error: Missing arguments.\n");
                    break;
                }

                // Вызов обработчика команды
                command_list[i].handler(socket, args, nickname);
                break;
            }
        }

        if (!command_found) {
            // find_player(nickname)->bad_input_count++;
            send_message(socket, "Unknown command.\n");
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < MAX_SAVED_PLAYERS; i++) {
                if (clients[i].socket == socket) {
                    clients[i].bad_command_count++; // Увеличиваем счетчик
                    if (clients[i].bad_command_count >= 5) {
                        printf("DISCONNECTED: Too many invalid commands.");
                        send_message(socket, "DISCONNECTED: Too many invalid commands.");
                        clients[i].is_connected = false;
                        close(socket); // Закрываем соединение
                        break;
                    }
                }
            }
            pthread_mutex_unlock(&clients_mutex);
        }else {
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < MAX_SAVED_PLAYERS; i++) {
                if (clients[i].socket == socket) {
                    clients[i].bad_command_count = 0; // Сбрасываем счетчик
                    break;
                }
            }
            pthread_mutex_unlock(&clients_mutex);
        }

        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_SAVED_PLAYERS; i++) {
            if (clients[i].socket == socket) {
                clients[i].last_ping_time = time(NULL);
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);
    }

    // printf("Client disconnected.\n");
    // pthread_mutex_lock(&clients_mutex);
    // for (int i = 0; i < MAX_SAVED_PLAYERS; i++) {
    //     if (clients[i].socket == socket) {
    //         clients[i].is_connected = false;
    //         clients[i].socket = -1;
    //         // break; //123
    //     }
    // }
    // pthread_mutex_unlock(&clients_mutex);

    //123
    if (valread == 0) {
        printf("Client disconnected.\n");
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_SAVED_PLAYERS; i++) {
            if (clients[i].socket == socket) {
                clients[i].is_connected = false;
                clients[i].socket = -1;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);
        close(socket);
        pthread_exit(NULL);
    }
    //123

    close(socket);
    pthread_exit(NULL);
    // return NULL; 123
}

Player* find_player(const char* nickname) {
    // printf("Finding player...\n");
    for (int i = 0; i < room_count; i++) {
        for (int j = 0; j < rooms[i].player_count; j++) {
            if (strcmp(rooms[i].players[j].nickname, nickname) == 0) {
                return &rooms[i].players[j];
            }
        }
    }
    printf("Player not found1.\n");
    return NULL;
}

ClientInfo* find_player_socket(int socket) {
        for (int j = 0; j < MAX_SAVED_PLAYERS; j++) {
            if (clients[j].socket == socket) {
                return &clients[j];
            }
        }
    // printf("Player not found2.\n");
    return NULL;
}

void create_room(int client_socket, const char* nickname) {
    pthread_mutex_lock(&room_mutex);
    if (room_count >= MAX_ROOMS) {
        send_message(client_socket, "WARNING:MAX_ROOMS");
        pthread_mutex_unlock(&room_mutex);
        return;
    }

    const Player *tmp_player = find_player(nickname);
    if (tmp_player != NULL) {
        printf("player:%p found\n",tmp_player);
        if(tmp_player->status == IN_LOBBY || tmp_player->status == IN_GAME) {
            send_message(client_socket,"Already in the game.");
            pthread_mutex_unlock(&room_mutex);
            return;
        }
    }

    printf("here1");
    Room new_room;
    snprintf(new_room.room_name, sizeof(new_room.room_name), "%s's Room", nickname);
    new_room.player_count = 1;
    new_room.game_started = false;

    // strncpy(new_room.secret_number, "1345", sizeof(new_room.secret_number) - 1);

    // Инициализация первого игрока
    new_room.players[0].socket = client_socket;
    strncpy(new_room.players[0].nickname, nickname, sizeof(new_room.players[0].nickname));
    new_room.players[0].is_connected = true;
    new_room.players[0].status = IN_LOBBY;
    new_room.players[0].secret = -1;
    new_room.players[0].last_ping_time = time(NULL);

    new_room.saved_player_count = 0;

    int room_index = room_count;
    rooms[room_index] = new_room;
    room_count++;

    printf("Room created: %s\n", new_room.room_name);


    // // Создаем поток для проверки пингов
    // pthread_t ping_thread;
    // int *room_index_ptr = malloc(sizeof(int)); // Создаем копию индекса для передачи в поток
    // if (room_index_ptr == NULL) {
    //     perror("Failed to allocate memory for room index");
    //     pthread_mutex_unlock(&room_mutex);
    //     return;
    // }
    // *room_index_ptr = room_index;
    //
    // if (pthread_create(&ping_thread, NULL, check_player_pings, room_index_ptr) != 0) {
    //     perror("Failed to create ping thread");
    //     free(room_index_ptr); // Освобождаем память, если поток не был создан
    //     pthread_mutex_unlock(&room_mutex);
    //     return;
    // }


    // Отправка списка никнеймов клиенту
    char nickname_list[BUFFER_SIZE] = "NICKNAMES:";
    strcat(nickname_list, nickname);
    strcat(nickname_list, "\n");
    printf("here2");
    send(client_socket, nickname_list, strlen(nickname_list), 0);

    pthread_mutex_unlock(&room_mutex);
}

void join_room(int client_socket, const char* room_name, const char* nickname) {
    pthread_mutex_lock(&room_mutex);  // Включите блокировку для синхронизации
    const Player *tmp_player = find_player(nickname);
    printf("player:%p found1\n",tmp_player);
    if (tmp_player != NULL) {
        if(tmp_player->status == IN_LOBBY || tmp_player->status == IN_GAME) {
            send_message(client_socket, "WARN:IN_GAME_ALRDY\n");
            pthread_mutex_unlock(&room_mutex);  // Включите блокировку для синхронизации
            return;
        }
    }


    for (int i = 0; i < room_count; i++) {
        printf("room-count-%d-\n",rooms[i].player_count);
        if(rooms[i].player_count == 0) {
            send_message(client_socket, "WARN:ROOM_NO_EXIST\n");
            printf("Room alrdy dltd\n");
            return;
        }
        if (strcmp(rooms[i].room_name, room_name) == 0) {
            if (rooms[i].player_count < MAX_PLAYERS_PER_ROOM) {

                // printf("if 2 passed %s\n",nickname);

                // Проверяем, сохранен ли игрок
                // int saveResult = save_player_to_room(&rooms[i], nickname);
                int saveResult = check_save_status(&rooms[i], nickname);


                // printf("==============================\n");
                // printf("--------------room--------%d\n",saveResult);
                // printf("==============================\n");
                if (saveResult == 1) { // Игрок уже сохранён, это реконнект

                    for (int j = 0; j < MAX_PLAYERS_PER_ROOM; j++) {
                        int opponent_index = (j == 0) ? 1 : 0;
                        if (strcmp(rooms[i].players[j].nickname, nickname) == 0) {

                            // Обновляем информацию о переподключившемся игроке
                            rooms[i].players[j].socket = client_socket;
                            rooms[i].players[j].is_connected = true;
                            rooms[i].players[j].pending_reconnect = false;
                            rooms[i].players[j].last_ping_time = time(NULL);
                            rooms[i].players[j].status = IN_GAME;
                            rooms[i].player_count++;

                            printf("Player %s reconnected to room %s.\n", nickname, room_name);
                            rooms[i].players[j].in_game = true;

                            if(!rooms[i].players[opponent_index].is_turn){
                                rooms[i].players[j].is_turn = true;
                                send_message(rooms[i].players[j].socket, "UNLOCK_GUESS");
                            }

                            send_message(rooms[i].players[j].socket, "RECONNECTED");


                        } else if (rooms[i].players[j].is_connected) {
                            // Уведомляем других подключённых игроков о реконнекте
                            send_message(rooms[i].players[j].socket, "OPPONENT_RECONNECTED");
                            // opponent_index = (j == 0) ? 1 : 0;
                            if(!rooms[i].players[j].is_turn){
                                rooms[i].players[opponent_index].is_turn = true;
                                send_message(rooms[i].players[opponent_index].socket, "UNLOCK_GUESS");
                            }

                        }
                    }

                    pthread_mutex_unlock(&room_mutex);
                    return;
                }else if (saveResult == 0) { // Новый игрок
                    // Найдите первый свободный слот для нового игрока
                    for (int j = 0; j < MAX_PLAYERS_PER_ROOM; j++) {
                        if (!rooms[i].players[j].is_connected) {
                            // Инициализация нового игрока
                            rooms[i].players[j].socket = client_socket;
                            rooms[i].players[j].is_connected = true;
                            rooms[i].players[j].status = IN_LOBBY;
                            rooms[i].players[j].secret = -1;
                            strncpy(rooms[i].players[j].nickname, nickname, sizeof(rooms[i].players[j].nickname) - 1);

                            break;
                        }
                    }

                    rooms[i].player_count++;
                    printf("Player joined room: %s. Total players: %d\n", room_name, rooms[i].player_count);

                    // Уведомление всех клиентов о количестве игроков
                    for (int j = 0; j < MAX_PLAYERS_PER_ROOM; j++) {
                        if (rooms[i].players[j].is_connected) {
                            send_player_count(rooms[i].players[j].socket, room_name);
                        }
                    }

                    // Обновляем список никнеймов
                    char nickname_list[BUFFER_SIZE] = "NICKNAMES:";
                    for (int j = 0; j < MAX_PLAYERS_PER_ROOM; j++) {
                        if (rooms[i].players[j].is_connected) {
                            strcat(nickname_list, rooms[i].players[j].nickname);
                            strcat(nickname_list, ",");
                        }
                    }

                    // Удаляем последнюю запятую
                    size_t len = strlen(nickname_list);
                    if (len > 0 && nickname_list[len - 1] == ',') {
                        nickname_list[len - 1] = '\0';
                    }
                    strcat(nickname_list, "\n");

                    // Отправляем обновленный список никнеймов всем игрокам
                    for (int j = 0; j < MAX_PLAYERS_PER_ROOM; j++) {
                        if (rooms[i].players[j].is_connected) {
                            send(rooms[i].players[j].socket, nickname_list, strlen(nickname_list), 0);
                        }
                    }

                    send_message(client_socket, "JOIN_SUCCESS");
                    pthread_mutex_unlock(&room_mutex);
                    return;
                } else { // Ошибка сохранения игрока
                    send_message(client_socket, "ROOM_ERROR");
                    pthread_mutex_unlock(&room_mutex);
                    return;
                }
            } else { // Комната заполнена
                send_message(client_socket, "ROOM_FAIL");
                pthread_mutex_unlock(&room_mutex);
                return;
            }
        }
    }
    send_message(client_socket, "ROOM_FAIL");

    pthread_mutex_unlock(&room_mutex);
    send_message(client_socket, "ROOM_NOT_FOUND");

}

void send_room_list(int client_socket, const char* nickname) {
    pthread_mutex_lock(&room_mutex);

    if (room_count < 0 || room_count > MAX_ROOMS) {
        printf("Error: room_count is out of bounds (%d).\n", room_count);
        send_message(client_socket, "ROOM_LIST:");
        pthread_mutex_unlock(&room_mutex);
        return;
    }

    printf("_-_-_-_-_-_-_-_-_-_-_-_-_-_-_\n");
    char room_list[BUFFER_SIZE] = "ROOM_LIST:";
    char room_info[50];
    printf("Starting to process rooms (room_count=%d)\n", room_count);

    for (int i = 0; i < room_count; i++) {
        printf("Processing room %d\n", i);
        printf("Room %d: room_name=%s, player_count=%d, saved_player_count=%d\n",
               i, rooms[i].room_name, rooms[i].player_count, rooms[i].saved_player_count);

        if (rooms[i].player_count >= MAX_PLAYERS_PER_ROOM) {
            continue;
        }

        if (rooms[i].saved_player_count == 0) {
            int written = snprintf(room_info, sizeof(room_info), "%s,", rooms[i].room_name);
            if (written < 0 || written >= (int)sizeof(room_info)) {
                printf("Error: snprintf failed for room %d\n", i);
                continue;
            }

            if (strlen(room_list) + strlen(room_info) >= BUFFER_SIZE - 1) {
                printf("Error: room_list buffer overflow detected.\n");
                break;
            }
            strcat(room_list, room_info);
        } else {
            for (int j = 0; j < rooms[i].saved_player_count; j++) {
                if (strcmp(rooms[i].saved_nicknames[j], nickname) == 0) {
                    int written = snprintf(room_info, sizeof(room_info), "%s,", rooms[i].room_name);
                    if (written < 0 || written >= (int)sizeof(room_info)) {
                        printf("Error: snprintf failed for room %d, saved player %d\n", i, j);
                        continue;
                    }

                    if (strlen(room_list) + strlen(room_info) >= BUFFER_SIZE - 1) {
                        printf("Error: room_list buffer overflow detected.\n");
                        break;
                    }
                    strcat(room_list, room_info);
                }
            }
        }
    }

    printf("_-_-_-_-_-_-_-_-_-_-_-_-_-_-_END\n");
    send_message(client_socket, room_list);
    pthread_mutex_unlock(&room_mutex);
}

void send_player_count(int client_socket, const char* room_name) {
    // pthread_mutex_lock(&room_mutex);
    if(sizeof(room_name) == 0) {
        printf("=========================HUEVO================================");
    }
    // printf("Requested room name for player count: %s\n", room_name);

    for (int i = 0; i < room_count; i++) {
        if (strcmp(rooms[i].room_name, room_name) == 0) {
            char message[BUFFER_SIZE];
            snprintf(message, sizeof(message), "PLAYER_COUNT:%d", rooms[i].player_count);
            // printf(message);
            send_message(client_socket, message);
            // pthread_mutex_unlock(&room_mutex);
            return;
        }
    }
    // pthread_mutex_unlock(&room_mutex);
    printf("not found3");
    send_message(client_socket, "ROOM_NOT_FOUND");
}

void send_message(int client_socket, const char *message) {
    if (message == NULL || strlen(message) == 0) {
        return; // Не отправлять пустое сообщение
    }
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "%s\n", message);
    send(client_socket, buffer, strlen(buffer), 0);
}

void leave_room(int client_socket) {
    pthread_mutex_lock(&room_mutex);
    for (int i = 0; i < room_count; i++) {
        for (int j = 0; j < MAX_PLAYERS_PER_ROOM; j++) {
            printf("Checking player in room %s: socket = %d, client_socket = %d, is_connected = %d\n",
                   rooms[i].room_name, rooms[i].players[j].socket, client_socket, rooms[i].players[j].is_connected);

            if (rooms[i].players[j].socket == client_socket && rooms[i].players[j].is_connected) {
                if(rooms[i].players[j].status == IN_GAME) {
                    send_message(client_socket,"WARN: LEAVE(_ROOM)");
                    pthread_mutex_unlock(&room_mutex);
                    return;
                }
                printf("Player found, removing from room\n");
                rooms[i].players[j].in_game = false;
                rooms[i].players[j].is_connected = false;
                rooms[i].players[j].socket = -1; // Сброс сокета
                rooms[i].players[j].status = CHOOSING_ROOM; // Сброс сокета
                rooms[i].player_count--;
                printf("Player left room: %s. Total players: %d\n", rooms[i].room_name, rooms[i].player_count);

                send_message(client_socket,"SUCCESS:LEFT");

                // Обновление и отправка списка никнеймов оставшимся клиентам
                char nickname_list[BUFFER_SIZE] = "NICKNAMES:";
                for (int k = 0; k < MAX_PLAYERS_PER_ROOM; k++) {
                    if (rooms[i].players[k].is_connected) {
                        strcat(nickname_list, rooms[i].players[k].nickname);
                        strcat(nickname_list, ",");
                    }
                }

                // Удаление последней запятой и добавление символа новой строки
                size_t len = strlen(nickname_list);
                if (len > 0 && nickname_list[len - 1] == ',') {
                    nickname_list[len - 1] = '\0'; // Убираем последнюю запятую
                }
                strcat(nickname_list, "\n");

                // Отправка обновленного списка никнеймов всем оставшимся подключенным клиентам
                for (int k = 0; k < MAX_PLAYERS_PER_ROOM; k++) {
                    if (rooms[i].players[k].is_connected) {
                        send(rooms[i].players[k].socket, nickname_list, strlen(nickname_list), 0);
                        send_player_count(rooms[i].players[k].socket, rooms[i].room_name);
                    }
                }

                // Очистка комнаты, если она пустая
                if (rooms[i].player_count <= 0) {
                    printf("Room %s is now empty.\n", rooms[i].room_name);
                    memset(&rooms[i], 0, sizeof(Room)); // Сброс комнаты (по желанию)
                    send_message(client_socket,"Room dlt (room is empty)");
                    for (int z = 0; z < rooms[i].saved_player_count; z++) {
                        send_message(client_socket, "ROOM_CLEARED");
                    }
                    room_count--;
                }

                pthread_mutex_unlock(&room_mutex);
                return;
            }
        }
    }
    pthread_mutex_unlock(&room_mutex);
}

bool calculate_bulls_and_cows(const char* secret, const char* guess, int* bulls, int* cows) {
    *bulls = 0;
    *cows = 0;

    int secret_counts[10] = {0};
    int guess_counts[10] = {0};

    for (int i = 0; i < 4; i++) {
        if (secret[i] == guess[i]) {
            (*bulls)++;
        } else {
            secret_counts[secret[i] - '0']++;
            guess_counts[guess[i] - '0']++;
        }
    }

    for (int i = 0; i < 10; i++) {
        *cows += (secret_counts[i] < guess_counts[i]) ? secret_counts[i] : guess_counts[i];
    }
    printf("bulls: %d\n", *bulls);
    printf("cows: %d\n", *cows);
    return (*bulls == 4); // Возвращаем true, если игрок угадал
}

void* check_player_pings(void* arg) {
    while (1) {
        pthread_mutex_lock(&clients_mutex);
        time_t current_time = time(NULL);

        for (int i = 0; i < MAX_SAVED_PLAYERS; i++) {
            if (clients[i].is_connected) {
                if (difftime(current_time, clients[i].last_ping_time) > 10) { // 60 секунд без пинга
                    printf("Client %d timed out. Disconnecting...\n", clients[i].socket);

                    printf("-%s-\n", clients[i].nickname);
                    const Player *tmp_player = find_player(clients[i].nickname);

                    if (tmp_player != NULL) {
                        printf("\n tmp notnull\n");
                        if(tmp_player->status == IN_GAME) {
                            // send_message(tmp_player->socket,"LEAVE_GAME");
                            printf("\n tmp not_ion_game_now\n");
                            leave_game(clients[i].socket);
                        }
                        if(tmp_player->status == IN_LOBBY) {
                            // send_message(tmp_player->socket,"LEAVE");
                            printf("\n tmp not_ion_room_now\n");
                            leave_room(clients[i].socket);
                        }
                    }

                    // Отправляем сообщение и закрываем соединение
                    send_message(clients[i].socket, "DISCONNECTED: Timed out (inactivity)");
                    close(clients[i].socket);
                    clients[i].is_connected = false;
                    clients[i].socket = -1;
                }
            }
        }
        pthread_mutex_unlock(&clients_mutex);
        sleep(5); // Проверяем каждые 5 секунд
    }
    return NULL;
}

void leave_game(int client_socket) {
    pthread_mutex_lock(&room_mutex);

    for (int i = 0; i < room_count; i++) {
        for (int j = 0; j < MAX_PLAYERS_PER_ROOM; j++) {
            if (rooms[i].players[j].socket == client_socket && rooms[i].players[j].is_connected) {

                if(rooms[i].players[j].status != GAME_OVER) {
                    if(rooms[i].players[j].status != IN_GAME) {
                        send_message(client_socket,"Game has not started yet");
                        pthread_mutex_unlock(&room_mutex);
                        return;
                    }
                    // send_message(client_socket,"Game ended - leaving the room...");
                    // pthread_mutex_unlock(&room_mutex);
                    // return;
                }



                // if(rooms[i].players[j].status != GAME_OVER) {
                //     send_message(client_socket,"Game has not started yet");
                //     pthread_mutex_unlock(&room_mutex);
                //     return;
                // }


                // if(!rooms[i].game_started) {
                //     send_message(client_socket,"Game has not started yet");
                //     pthread_mutex_unlock(&room_mutex);
                //     return;
                // }



                printf("Player %s left game in room %s.\n", rooms[i].players[j].nickname, rooms[i].room_name);

                // Уведомляем второго игрока
                int opponent_index = (j == 0) ? 1 : 0;
                if (rooms[i].players[opponent_index].is_connected) {
                    send_message(rooms[i].players[opponent_index].socket, "OPPONENT_LEFT:Your opp left: reconnecting...");
                }

                // Помечаем игрока как временно отключённого
                rooms[i].players[j].is_connected = false;
                rooms[i].players[j].pending_reconnect = true;
                rooms[i].players[j].disconnect_time = time(NULL);
                rooms[i].players[j].in_game = false;
                rooms[i].players[j].status = CHOOSING_ROOM;
                rooms[i].player_count--;


                // rooms[i].saved_player_count--;

                // Если оба игрока отключены или время истекло, закрываем комнату
                // if (!rooms[i].players[opponent_index].is_connected && !rooms[i].players[opponent_index].pending_reconnect) {
                //     printf("Both players disconnected. Closing room: %s.\n", rooms[i].room_name);
                //     memset(&rooms[i], 0, sizeof(Room));
                //     room_count--;
                // }

                if (rooms[i].player_count == 0) {
                    printf("Room %s is now empty.\n", rooms[i].room_name);
                    for (int z = 0; z < rooms[i].saved_player_count; z++) {
                        send_message(rooms[i].saved_sockets[z], "ROOM_CLEARED");
                    }
                    memset(&rooms[i], 0, sizeof(Room)); // Сброс комнаты (по желанию)
                    room_count--;
                    send_message(client_socket,"No players. Room deleted");
                }

                pthread_mutex_unlock(&room_mutex);
                return;
            }
        }
    }
    send_message(client_socket,"NO ROOM WITH RUNNING GAME");
    pthread_mutex_unlock(&room_mutex);
}

int save_player_to_room(Room* room, const char* nickname) {
    // Проверяем, не сохранен ли уже игрок
    if(check_save_status(room,nickname) == 1) {
        return 1;
    }

    // Если места для сохранения нет, проверяем лимит
    if (room->saved_player_count >= MAX_PLAYERS_PER_ROOM) {
        printf("[ERROR] Room %s is full. Cannot save player %s.\n", room->room_name, nickname);
        return -1; // Комната заполнена
    }

    const Player *tmp_player = find_player(nickname);
    printf("player:%p found1\n",tmp_player);
    int tmp_socket = -1;
    if (tmp_player != NULL) {
        tmp_socket = tmp_player->socket;
    }

    // Сохраняем игрока
    // if(room->game_started) {
        size_t nickname_length = sizeof(room->saved_nicknames[room->saved_player_count]);
        strncpy(room->saved_nicknames[room->saved_player_count], nickname, nickname_length - 1);
        room->saved_nicknames[room->saved_player_count][nickname_length - 1] = '\0'; // Завершаем строку
        if (tmp_socket != -1) {
            room->saved_sockets[room->saved_player_count] = tmp_socket;
        }

        room->saved_player_count++;

        printf("[DEBUG] Player %s saved in room %s. Total saved players: %d.\n", nickname, room->room_name, room->saved_player_count);
        // Успешно сохранен
    //     return 0;
    // }

    return 0;

}

int check_save_status(Room* room, const char* nickname) {
    for (int i = 0; i < room->saved_player_count; i++) {
        if (strcmp(room->saved_nicknames[i], nickname) == 0) {
            printf("[DEBUG] Player %s already saved in room %s.\n", nickname, room->room_name);
            return 1; // Игрок уже сохранен
        }
    }
    return 0;
}

bool isAsciiString(const char *str) {
    if (!str || *str == '\0') {
        return false; // Пустая строка недопустима
    }

    for (size_t i = 0; str[i] != '\0'; i++) {
        // Допустимы буквы, цифры, пробелы, подчеркивания, тире и двоеточие
        if (!isalnum(str[i]) && str[i] != '_' && str[i] != '-' && str[i] != ':' && str[i] != ' ') {
            return false;
        }
    }

    return true; // Все символы допустимы
}

//============================= HANDLERS ===================================

void handle_leave_game(int socket, const char *args, char *nickname) {
    printf("%c", *args);
    printf("Player %s is leaving the game.\n", nickname);
    leave_game(socket);
}

void handle_create_lobby(int socket, const char *args, char *nickname) {
    printf("%c", *args);
    printf("Player %s trying create a lobby.\n", nickname);
    create_room(socket, nickname);
    send_room_list(socket, nickname);
}

void handle_user_secret(int socket, const char *args, char *nickname) {
    int secret = 0;

    // printf("Player %s set secret: %d\n", nickname, secret);
    if(isValidGuess(args)) {
        secret = atoi(args);
    }else {
        send_message(socket,"Invalid secret number - use 4 unique numbers");
        return;
    }

    if(secret == 0) {
        send_message(socket,"Smth went wrong... try again");
        printf("Smth went wrong... try again\n");
        return;
    }

    // Найти комнату и сохранить секрет
    for (int i = 0; i < room_count; i++) {
        for (int j = 0; j < MAX_PLAYERS_PER_ROOM; j++) {
            if (strcmp(rooms[i].players[j].nickname, nickname) == 0) {

                if(rooms[i].players[j].status != IN_LOBBY) {
                    send_message(socket,"Unable to change a secret number now");
                    return;
                }

                // if(rooms[i].game_started) {
                //     send_message(socket,"Game already started - unuble to change a secret number");
                //     return;
                // }

                rooms[i].players[j].secret = secret;
                send_message(socket, "SECRET_RECEIVED");
                return;
            }
        }
    }

    send_message(socket, "Error: Room not found.\n");
}

bool handle_nickname(int socket, const char *args, char *nickname) { //123
    int test_tmp_pocet = testClinetInfo(nickname);
    printf("TOTAL=CLIENT=COUNT===%d",test_tmp_pocet);
    if (is_nickname_taken(nickname)) {
        send_message(socket, "NICKNAME_TAKEN");

        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_SAVED_PLAYERS; i++) {
            if (clients[i].socket == socket) {
                    clients[i].is_connected = false;
                    close(socket); // Закрываем соединение
                    break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);
        return false; //123
    }

    // strncpy(nickname, args, 49); // Сохраняем никнейм
    // nickname[49] = '\0';

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_SAVED_PLAYERS; i++) {
        if (clients[i].socket == socket) {
            strncpy(clients[i].nickname, nickname, sizeof(clients[i].nickname) - 1);
            clients[i].nickname[sizeof(clients[i].nickname) - 1] = '\0'; // Завершаем строку
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    printf("Player set nickname: %s\n", nickname);
    send_message(socket, "NICKNAME_SET");
    return true; //123
}

void handle_ping(int socket, const char *args, char *nickname) {
    printf("%c", *args);
    printf("Received PING from %s.\n", nickname);
    send_message(socket, "PONG");

    pthread_mutex_lock(&room_mutex);
    Player *player = find_player(nickname);
    if (player != NULL) {
        player->last_ping_time = time(NULL);
    }
    pthread_mutex_unlock(&room_mutex);
}

void handle_join_room(int socket, const char *args, char *nickname) {
    join_room(socket, args, nickname);
}

void handle_leave_room(int socket, const char *args, char *nickname) {
    printf("%c", *args);
    // printf("Player %s is leaving the room.\n", nickname);
    leave_room(socket);
}

void handle_get_player_count(int socket, const char *args, char *nickname) {
    printf("Player %s requested player count for room: %s\n", nickname, args);
    send_player_count(socket, args);
}

void handle_get_rooms(int socket, const char *args, char *nickname) {
    printf("%c", *args);
    printf("Player %s requested room list.\n", nickname);
    send_room_list(socket, nickname);
}

void handle_start_game(int socket, const char *args, char *nickname) {
    printf("%c", *args);
    printf("Player %s is trying to start a game.\n", nickname);
        // pthread_mutex_lock(&room_mutex);  // Блокировка комнаты
    for (int i = 0; i < room_count; i++) {
        for (int j = 0; j < MAX_PLAYERS_PER_ROOM; j++) {
            if (rooms[i].players[j].socket == socket && rooms[i].players[j].is_connected) {
                if(rooms[i].player_count != 2) {
                    send_message(socket,"2 players needed to start game");
                    return;
                }

                if(rooms[i].players[j].secret == -1) {
                    send_message(socket,"Set secret number first");
                    return;
                }

                if(rooms[i].game_started) {
                    send_message(socket,"Game already started");
                    return;
                }
                rooms[i].players[j].ready_to_start = true; // Игрок нажал "Start Game"
                printf("Player %s in room %s is ready to start.\n", rooms[i].players[j].nickname, rooms[i].room_name);

                // Проверяем, готовы ли все игроки
                bool all_ready = true;
                for (int k = 0; k < MAX_PLAYERS_PER_ROOM; k++) {
                    if (rooms[i].players[k].is_connected && !rooms[i].players[k].ready_to_start) {
                        all_ready = false;
                        break;
                    }
                }



                // Если все игроки готовы, начинаем игру
                if (all_ready) {
                    rooms[i].game_started = true;

                    // Устанавливаем очередь — первый игрок начинает
                    for (int k = 0; k < MAX_PLAYERS_PER_ROOM; k++) {
                        rooms[i].players[k].is_turn = (k == 0); // Первый игрок (индекс 0) начинает
                        if(rooms[i].players[k].is_turn) {
                            send_message(rooms[i].players[k].socket,"UNLOCK_GUESS");
                        }
                    }

                    // Уведомляем игроков о старте игры
                    for (int k = 0; k < MAX_PLAYERS_PER_ROOM; k++) {
                        // rooms[i].players[k].status = IN_GAME;
                        send_message(rooms[i].players[k].socket, "GAME_STARTED");
                        send_message(rooms[i].players[k].socket, "RECONNECT"); // Уведомляем клиента о реконнекте
                        // printf("---recon2---");
                        save_player_to_room(&rooms[i], rooms[i].players[k].nickname);
                        printf("Game started for room: %s\n", rooms[i].room_name);
                        rooms[i].players[k].status = IN_GAME;

                    }
                    return;
                }else {
                    // Уведомляем текущего игрока, что он готов
                    send_message(socket, "WAITING_FOR_OTHER_PLAYER");
                    return;
                }
                // pthread_mutex_unlock(&room_mutex);
                // return;
            }
        }
    }
    // pthread_mutex_unlock(&room_mutex);
    printf("not found1");
    send_message(socket, "ROOM_NOT_FOUND");
}

void handle_guess(int socket, const char *args, char *nickname) {
    if (!isValidGuess(args)) {
        send_message(socket, "Invalid_guess. Please_enter_a_4-digit_number_with_unique_digits.");
        return;
    }
    for (int i = 0; i < room_count; i++) {
        for (int j = 0; j < MAX_PLAYERS_PER_ROOM; j++) {
            if (rooms[i].players[j].socket == socket) {
                if (!rooms[i].game_started) {
                    send_message(socket, "Game_has_not_started_yet.");
                    return;
                }

                if (!rooms[i].players[j].is_turn) {
                    send_message(socket, "It's_not_your_turn.");
                    return;
                }
                printf("Player %s made a guess: [%s]\n", nickname, args);
                int opponent_index = (j == 0) ? 1 : 0;
                int bulls = 0, cows = 0;

                char opponent_secret[5];
                snprintf(opponent_secret, 5, "%04d", rooms[i].players[opponent_index].secret);

                if (calculate_bulls_and_cows(opponent_secret, args, &bulls, &cows)) {
                    send_message(socket, "===WINNER===");
                    send_message(rooms[i].players[opponent_index].socket, "===LOOSER===");

                    rooms[i].game_started = false;
                    for (int j = 0; j < MAX_PLAYERS_PER_ROOM; j++) {
                            rooms[i].players[j].status = GAME_OVER;
                            leave_game(rooms[i].players[j].socket);
                    }
                } else {
                    char result[BUFFER_SIZE];
                    snprintf(result, BUFFER_SIZE, "RESULT:%d Bulls, %d Cows", bulls, cows);
                    send_message(socket, result);
                    send_message(rooms[i].players[opponent_index].socket, "UNLOCK_GUESS");
                }

                rooms[i].players[j].is_turn = false;
                rooms[i].players[opponent_index].is_turn = true;
                send_message(rooms[i].players[opponent_index].socket, "Your_turn.");
                return;
            }
        }
    }

    send_message(socket, "ROOM_NOT_FOUND");
}

bool isValidGuess(const char *guess) {
    if (strlen(guess) != 4) { // Длина должна быть ровно 4 символа
        return false;
    }

    bool digits[10] = {false}; // Массив для отслеживания уникальных цифр

    for (int i = 0; i < 4; i++) {
        if (!isdigit(guess[i])) { // Проверяем, является ли символ цифрой
            return false;
        }

        int digit = guess[i] - '0';
        if (digits[digit]) { // Если цифра уже встречалась, невалидно
            return false;
        }
        digits[digit] = true; // Отмечаем цифру как встреченную
    }

    return true; // Все проверки пройдены
}

bool is_nickname_taken(const char* nickname) {
    for (int i = 0; i < MAX_SAVED_PLAYERS; i++) {
        if (clients[i].is_connected && strcmp(clients[i].nickname, nickname) == 0) {
            return true; // Никнейм уже занят
        }
    }
    return false; // Никнейм свободен
}

int testClinetInfo(const char* nickname) {
    int pocet = 0;
    for (int i = 0; i < MAX_SAVED_PLAYERS; i++) {
        if(strcmp(clients[i].nickname, nickname) == 0) {
            pocet++;
        }
    }
    return pocet;
}

void close_server() {
    close(server_fd);
    exit(0);
}