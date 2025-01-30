# Название исполняемого файла
TARGET = server

# Исходные файлы
SRC = main.c

# Компилятор
CC = gcc

# Флаги компиляции (например, для отладки используйте -g)
CFLAGS = -Wall -Wextra -pthread

# Команда линковки (обычно не требуется изменять)
LDFLAGS = -pthread

# Правило для сборки исполняемого файла
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

# Очистка собранных файлов
clean:
	rm -f $(TARGET)

# Дополнительное правило для пересборки
rebuild: clean $(TARGET)
