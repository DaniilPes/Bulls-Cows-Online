import javax.swing.*;
import java.awt.*;
import java.awt.event.ActionListener;
import java.io.*;
import java.net.Socket;
import java.util.ArrayList;
import javax.swing.Timer;
public class BullsAndCowsClient extends JFrame {
    static String ip;
    static String port;
    JTextArea gameLogArea;
    private JTextField guessInputField;
    JButton guessButton = new JButton("Send");
    private JButton backButton;
    boolean connectionError = false;
    PrintWriter out;
    BufferedReader in;
    Socket socket;
    JList<String> roomList;
    JFrame roomSelectionFrame,roomFrame,gameFrame;
    boolean isGameStarted = false,isReconnect = false,isRoomSelectionOpen = false;
    ArrayList<String> logs;
    boolean guess_unlock = false;
    int playerCount;
    JLabel playerCountLabel = new JLabel("123");
    boolean running;
    String roomsData,roomsDataresponse,nickname;
    private String userSecretNumber;
    JLabel player1Label,player2Label;
    Timer pingTimer,pingCheckTimer;
    Thread serverListener;
    long lastPongTime;
    String savedRoom = "-";
    boolean isOppReconnected;
    boolean leave;
    boolean clup = false;

    boolean pinging = true;
    public BullsAndCowsClient(String host, String port) {
        gameLogArea = new JTextArea();
        gameLogArea.setEditable(false);
        logs = new ArrayList<>();
        guessButton.setEnabled(false);
        try {
            socket = new Socket(host, Integer.parseInt(port));
            out = new PrintWriter(socket.getOutputStream(), true);
            in = new BufferedReader(new InputStreamReader(socket.getInputStream()));

            while (nickname == null || nickname.trim().isEmpty()) {
                nickname = JOptionPane.showInputDialog(this, "Enter your nickname:");

                if (nickname == null) {
                    // Пользователь закрыл диалоговое окно, можно выйти из цикла
                    JOptionPane.showMessageDialog(this, "Nickname cannot be empty. Please try again.");
                } else if (nickname.trim().isEmpty()) {
                    JOptionPane.showMessageDialog(this, "Nickname cannot be empty or contain only spaces. Please try again.");
                }
            }

            out.println("NICKNAME:" + nickname);
            System.out.println("Sent nickname to server: " + nickname);
            String temp = in.readLine();


            if(temp.equals("CON_OK")){
                lastPongTime = System.currentTimeMillis(); // Инициализация времени последнего PONG
                running = true;
                serverListener = new Thread(new ServerListener(this));
                serverListener.start();
                temp = in.readLine();
                if(!temp.equals("NICKNAME_SET")){
                    System.out.println("Nickname taken - try to use different");
                    stopClient();
                    cleanupAndExit();
                }
                player1Label = new JLabel("Player 1: ");
                player2Label = new JLabel("Player 2: ");
            }else{
                cleanupAndExit();
                return;
            }


            startPinging();
//            startPongTimeoutCheck();
        } catch (IOException ex) {
            System.out.println("Unable to connect to server (try different nickname)");
            System.exit(0);
//            ex.printStackTrace();
        }

    }

    void showRoomSelectionWindow(String[] rooms) {
        if (isRoomSelectionOpen) {
            return;  // Предотвращает повторное открытие окна
        }

        SwingUtilities.invokeLater(() -> {
            isRoomSelectionOpen = true;  // Устанавливаем флаг
            roomSelectionFrame = new JFrame("Room Selection - " + nickname);
            roomSelectionFrame.setSize(500, 300);
            roomSelectionFrame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
            roomSelectionFrame.setLocationRelativeTo(null);

            roomList = new JList<>(rooms);
            JScrollPane roomScrollPane = new JScrollPane(roomList);

            JPanel buttonPanel = new JPanel();
            buttonPanel.setLayout(new BoxLayout(buttonPanel, BoxLayout.Y_AXIS));

            JButton createLobbyButton = new JButton("Create lobby");
            JButton connectButton = new JButton("Connect");
            JButton refreshButton = new JButton("Refresh");

            buttonPanel.add(createLobbyButton);
            buttonPanel.add(Box.createVerticalStrut(10));
            buttonPanel.add(connectButton);
            buttonPanel.add(Box.createVerticalStrut(10));
            buttonPanel.add(refreshButton);

            // Обработчик для кнопки "Создать лобби"
            createLobbyButton.addActionListener(e -> {
                leave = false;
                out.println("CREATE_LOBBY");
                try {
                    String response = in.readLine();
                    if (response != null && response.startsWith("ROOM_LIST:")) {
                        String roomData = response.substring("ROOM_LIST:".length());
                        String[] roomArray = roomData.isEmpty() ? new String[]{"auto-created room"} : roomData.split(",");

                        roomList.setListData(roomArray);

                        if (roomSelectionFrame != null) {
                            roomSelectionFrame.dispose();
                            isRoomSelectionOpen = false;  // Сбрасываем флаг
                        }

//                        player1Label.setText("Player 1: " + nickname);

                        showRoomWindow(roomArray[roomArray.length - 1]);
                        System.out.println("Lobby created. Waiting for another player...");
                    } else {
                        System.out.println("Failed to create lobby.");
                    }
                } catch (IOException ex) {
                    System.out.println("Error creating lobby.");
//                    ex.printStackTrace();
                }
            });

            // Обработчик для кнопки "Подключиться"
            connectButton.addActionListener(e -> {
                String selectedRoom = roomList.getSelectedValue();
//                out.println("GET_PLAYER_COUNT " + selectedRoom);
                if (selectedRoom != null) {
                    out.println("JOIN_ROOM:" + selectedRoom);

                    try {
                        Thread.sleep(500); // Ожидание 1 секунда
                    } catch (InterruptedException ex) {
//                        ex.printStackTrace();
                        System.out.println("room unnavailable");
                    }

                    System.out.println("ConError: " + connectionError);
                    if(connectionError){
                        JOptionPane.showMessageDialog(this,"Room is full or doesnt exist");
                        refreshRooms();
                        connectionError = false;
                        return;
                    }

                    if (gameLogArea != null) {
                        System.out.println("Connected to room: " + selectedRoom);
                    }

                    roomSelectionFrame.dispose();
                    isRoomSelectionOpen = false;  // Сбрасываем флаг для окна выбора комнаты

//                    player2Label.setText("Player 2: " + nickname);

                    System.out.println("-i-s--r-e-c-o-n-n-e-c-t-"+isReconnect+"---");
                    if(isReconnect){

                        try {
                            initializeGameWindow();
                        } catch (IOException ex) {
                            System.out.println("ERROR:UNABLE TO INITIALIZE GAME WINDOW");
//                            throw new RuntimeException(ex);
                        }
                        leave = false;
                        isReconnect = false;
                    }else {
                        leave = false;
                        showRoomWindow(selectedRoom); // Показать окно комнаты
                    }

                } else {
                    JOptionPane.showMessageDialog(roomSelectionFrame, "Please select a room first.");
                }
            });


            // Обработчик для кнопки "Рефреш"
            refreshButton.addActionListener(e -> refreshRooms());

            JSplitPane splitPane = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT, buttonPanel, roomScrollPane);
            splitPane.setDividerLocation(150);
            roomSelectionFrame.add(splitPane);

            roomSelectionFrame.setVisible(true);


            roomSelectionFrame.addWindowListener(new java.awt.event.WindowAdapter() {
                @Override
                public void windowClosing(java.awt.event.WindowEvent e) {
                    System.out.println("Window closing detected."); // Для проверки
//                    out.println("LEAVE");
                    cleanupAndExit();
                }
            });
        });
    }

    void updateRoomList(String[] rooms) {
        SwingUtilities.invokeLater(() -> {
            if (roomList != null) {
                roomList.setListData(rooms);
                roomList.revalidate();
            }
        });
    }

    void validateSavedRoom(String[] rooms){
//        SwingUtilities.invokeLater(() -> {
        for (String room : rooms){
            if(room.equals(savedRoom)){
                return;
            }
        }
        isReconnect = false;
        gameLogArea.setText("");
        logs = new ArrayList<>();
        isOppReconnected = false;
        savedRoom ="-";
//        });

    }

    void refreshRooms() {
        // Запрос списка комнат на сервер
        out.println("GET_ROOMS");

        // Время ожидания данных от сервера (настраиваемое)
        long waitStartTime = System.currentTimeMillis();
        long waitTimeout = 1_000; // 1 секунд максимального ожидания

        // Ожидание получения данных от сервера
//        synchronized (this) {
//            while (roomsDataresponse == null) {
//                if (System.currentTimeMillis() - waitStartTime > waitTimeout) {
//                    System.out.println("Timeout while waiting for room data.");
//                    return;
//                }
//                try {
//                    this.wait(100); // Небольшая пауза ожидания с синхронизацией
//                } catch (InterruptedException e) {
//                    e.printStackTrace();
//                }
//            }
//        }

        // После получения данных
        if (roomsDataresponse != null && roomsDataresponse.startsWith("ROOM_LIST:")) {
            String roomData = roomsDataresponse.substring("ROOM_LIST:".length());
            String[] roomArray = roomData.isEmpty() ? new String[]{} : roomData.split(",");
            updateRoomList(roomArray); // Обновление списка комнат

            // Обработка, если нет доступных комнат
            if (roomArray.length == 0) {
                System.out.println("No available rooms.");
            }
            // Вызов окна выбора комнаты (даже если список пуст)
            showRoomSelectionWindow(roomArray);
        } else {
            showRoomSelectionWindow(new String[]{});
        }

        // Сброс данных
        roomsDataresponse = null;
    }

    void fetchRooms() {
        out.println("GET_ROOMS");
        if (roomSelectionFrame != null && !isRoomSelectionOpen) {
            return; // Уже отображается окно выбора комнаты, не открываем новое
        }
        try {
            String rooms = in.readLine();
            System.out.println("roomz:" + rooms);
            if (rooms != null && rooms.startsWith("ROOM_LIST:")) {
                String roomData = rooms.substring("ROOM_LIST:".length());
                String[] roomArray = roomData.isEmpty() ? new String[]{} : roomData.split(",");
                // Обработка, если нет доступных комнат
                if (roomArray.length == 0) {
                    System.out.println("No available rooms.");
                }
                validateSavedRoom(roomArray);

                // Вызов окна выбора комнаты (даже если список пуст)
                showRoomSelectionWindow(roomArray);
            } else {
                System.out.println("Error retrieving room list.");
            }
        } catch (IOException e) {
            if (gameLogArea != null) {
                System.out.println("Error retrieving room list.");
            }else{
                System.out.println("unable to get room list");
            }
//            e.printStackTrace();

        }
    }

    void initializeGameWindow() throws IOException {
        String player1 = player1Label.getText().substring("Player: 1".length()).trim();
        String player2 = player2Label.getText().substring("Player: 2".length()).trim();


        String me = nickname;
        String opponent = player1.equals(me) ? player2 : player1;


        gameFrame = new JFrame(me + " vs " + opponent + " | SN: ["+userSecretNumber+"]") ;
        gameFrame.setSize(400, 300);
        gameFrame.setDefaultCloseOperation(JFrame.DISPOSE_ON_CLOSE);
        gameFrame.setLocationRelativeTo(null);
        gameFrame.setLayout(new BorderLayout());

        guessInputField = new JTextField();
        guessInputField.setFont(new Font("Arial", Font.PLAIN, 16));
        gameFrame.add(guessInputField, BorderLayout.NORTH);

        gameLogArea.setFont(new Font("Arial", Font.PLAIN, 14));
        gameFrame.add(new JScrollPane(gameLogArea), BorderLayout.CENTER);

        JPanel buttonPanel = new JPanel();
        buttonPanel.setLayout(new FlowLayout());


        if (guess_unlock) {
            SwingUtilities.invokeLater(() -> guessButton.setEnabled(true));
        }
        JButton pingButton = new JButton("Start/stop ping");
        pingButton.addActionListener(e -> {
            if (pinging == true){
                pinging = false;
            }else {
                pinging = true;
            }
        });

        buttonPanel.add(guessButton);
        buttonPanel.add(pingButton);


        backButton = new JButton("Return");
        buttonPanel.add(backButton);

        gameFrame.add(buttonPanel, BorderLayout.SOUTH);

        for (ActionListener al : guessButton.getActionListeners()) {
            guessButton.removeActionListener(al);
        }
        guessButton.addActionListener(e -> {
            try {
                sendGuess();
            } catch (IOException ex) {
//                throw new RuntimeException(ex);
                System.out.println("ERROR:UNABLE TO SEND GUESS");
            }
        });

        backButton.addActionListener(e -> {
            out.println("LEAVE_GAME");
            leave = true;
            returnToRoomSelection();
        });

        gameFrame.addWindowListener(new java.awt.event.WindowAdapter() {
            @Override
            public void windowClosing(java.awt.event.WindowEvent e) {
                out.println("LEAVE_GAME");
                leave = true;
                cleanupAndExit();
            }
        });

        gameFrame.setVisible(true);

        // Закрываем окно комнаты
        if (roomFrame != null) {
            roomFrame.dispose();
            roomFrame = null;
        }
    }

    private void sendGuess() throws IOException {
        String guess = guessInputField.getText().trim();
        System.out.println("Input guess: " + guess);

        if (guess.matches("\\d{4}")  && hasUniqueDigits(guess)) { // Проверяем, является ли ввод 4-значным числом
            out.println("GUESS:" + guess);
//            out.flush(); // Обязательно сбрасываем буфер
            System.out.println("Sent to server: GUESS:" + guess);
            addLogs("You guessed: " + guess + "\n");
            guessButton.setEnabled(false);
            guess_unlock = false;
        } else {
            addLogs("Invalid input. Please enter a 4-digit number.\n");
        }

    }

    private int sendSecret() throws IOException {
        userSecretNumber = JOptionPane.showInputDialog("Enter your secret 4-digit number:");

        // Проверка на 4-значное число и уникальность цифр
        if (userSecretNumber != null && userSecretNumber.matches("\\d{4}") && hasUniqueDigits(userSecretNumber)) {
            out.println("USER_SECRET:" + userSecretNumber); // Отправляем секретное число на сервер
            addLogs("Your secret number: " + userSecretNumber + "\n");
            return 1; // Успех
        } else {
//            addLogs("Invalid input. Please enter a 4-digit number with unique digits.\n");
            return 0;
        }

    }

    private boolean hasUniqueDigits(String number) {
        boolean[] digits = new boolean[10]; // Массив для проверки цифр от 0 до 9

        for (char c : number.toCharArray()) {
            int digit = c - '0'; // Преобразуем символ в цифру
            if (digits[digit]) {
                return false; // Если цифра уже встречалась, возвращаем false
            }
            digits[digit] = true; // Отмечаем цифру как встреченную
        }
        return true; // Все цифры уникальны
    }

    void returnToRoomSelection() {
        if (gameFrame != null) {
            gameFrame.dispose(); // Закрываем игровое окно
            gameFrame = null; // Очищаем ссылку на окно
        }

        if (roomFrame != null) {
            roomFrame.dispose(); // Закрываем окно комнаты, если оно всё ещё открыто
            roomFrame = null;
        }
        guessButton.setEnabled(false); //123
        isRoomSelectionOpen = false; // Сбрасываем флаг выбора комнаты
        out.println("GET_ROOMS"); // Отправляем запрос на список комнат
    }

    void updatePlayerLabels(String[] nicknames) {
        SwingUtilities.invokeLater(() -> {
            // Очистка значений перед обновлением
            player1Label.setText("Player 1: ");
            player2Label.setText("Player 2: ");

            if (nicknames.length > 0) {
                player1Label.setText("Player 1: " + nicknames[0]);
            }
            if (nicknames.length > 1) {
                player2Label.setText("Player 2: " + nicknames[1]);
            }
        });
    }

    private void showRoomWindow(String roomName) {
        gameLogArea.setText("");
        roomFrame = new JFrame(roomName);
        savedRoom = roomName;
        roomFrame.setSize(300, 300);
        roomFrame.setMinimumSize(new Dimension(300,250));
        roomFrame.setDefaultCloseOperation(JFrame.DISPOSE_ON_CLOSE);
        roomFrame.setLocationRelativeTo(null);

        JLabel roomLabel = new JLabel("Room: " + roomName);
        playerCountLabel = new JLabel("Loading players...");

//        player1Label = new JLabel("Player 1: ");
//        player2Label = new JLabel("Player 2: ");

        String actualRoomName = roomName.replace("Your Room - ", "").trim();
        out.println("GET_PLAYER_COUNT:" + actualRoomName);

        JPanel panel = new JPanel();
        panel.setLayout(new BoxLayout(panel, BoxLayout.Y_AXIS));
        panel.add(roomLabel);
        panel.add(Box.createVerticalStrut(10));
        panel.add(playerCountLabel);

        panel.add(player1Label);
        panel.add(player2Label);

        JButton startGameButton = new JButton("Start Game");
        JButton returnButton = new JButton("Return");
        JButton setSecretNumber = new JButton("set secret number");
//        JButton pingButton = new JButton("Start/stop pinging");
//        pingButton.addActionListener(e -> {
//            if (pinging == true){
//                pinging = false;
//            }else {
//                pinging = true;
//            }
//        });



        setSecretNumber.addActionListener(e-> {
            int result = 0;
            try {
                result = sendSecret();
            } catch (IOException ex) {
//                throw new RuntimeException(ex);
                System.out.println("ERROR:UNABLE TO SEND  A SECRET_NUMBER ");
            }
            if(result == 1){
                startGameButton.setEnabled(true);
            }else{
                JOptionPane.showMessageDialog(
                        null,
                        "Must be a 4-digit number with unique numbers.",
                        "Wrong input",
                        JOptionPane.WARNING_MESSAGE
                );
            }
        });
        startGameButton.addActionListener(e -> {
            if (playerCount == 2){
                out.println("START_GAME");
                startGameButton.setEnabled(false); // Блокируем кнопку после нажатия
                System.out.println("You are ready. Waiting for the other player...\n");
            }else{
                JOptionPane.showMessageDialog(roomSelectionFrame, "to start the game you need two players.");
            }

        });
        returnButton.addActionListener(e -> {
//            out.println("GET_PLAYER_COUNT " + roomName);
            out.println("LEAVE_ROOM");
            returnToRoomSelection();
        });

        startGameButton.setEnabled(false);

//        panel.add(pingButton);
        panel.add(setSecretNumber);
        panel.add(startGameButton);
        panel.add(returnButton);

        roomFrame.add(panel);
        roomFrame.setVisible(true);

        roomFrame.addWindowListener(new java.awt.event.WindowAdapter() {
            @Override
            public void windowClosing(java.awt.event.WindowEvent e) {
                out.println("LEAVE");
                cleanupAndExit();
            }
        });
    }
    void cleanupAndExit() {
        System.out.println("Cleaning up...");
        running = false;
        clup = true;

        // Остановка серверного слушателя
        if (serverListener != null && serverListener.isAlive()) {
            try {
                serverListener.interrupt();
                serverListener.join(1000);
//                System.out.println("Server listener thread stopped.");
            } catch (InterruptedException e) {
//                e.printStackTrace();
                Thread.currentThread().interrupt(); // Восстанавливаем статус прерывания
            }
        }


        // Остановка таймеров
        if (pingTimer != null && pingTimer.isRunning()) {
            pingTimer.stop();
//            System.out.println("Ping timer stopped.");
        }

        if (pingCheckTimer != null && pingCheckTimer.isRunning()) {
            pingCheckTimer.stop();
//            System.out.println("Ping check timer stopped.");
        }

        // Закрытие GUI окон
        SwingUtilities.invokeLater(() -> {
            if (gameFrame != null) {
                gameFrame.dispose();
                gameFrame = null;
//                System.out.println("Game frame closed.");
            }

            if (roomFrame != null) {
                roomFrame.dispose();
                roomFrame = null;
//                System.out.println("Room frame closed.");
            }

            if (roomSelectionFrame != null) {
                roomSelectionFrame.dispose();
                roomSelectionFrame = null;
//                System.out.println("Room selection frame closed.");
            }
        });

        // Закрытие сокета и потоков ввода-вывода
        if (out != null) {
            try {
                out.println("LEAVE_ROOM");
                out.close();
//                System.out.println("Output stream closed.");
            } catch (Exception e) {
                System.err.println("Error closing output stream");
            }
        }

        if (in != null) {
            try {
                in.close();
//                System.out.println("Input stream closed.");
            } catch (IOException e) {
                System.err.println("Error closing input stream");
            }
        }

        if (socket != null && !socket.isClosed()) {
            try {
                socket.close();
//                System.out.println("Socket closed.");
            } catch (IOException e) {
                System.err.println("Error closing socket");
            }
        }

        // Завершение приложени
        System.exit(0);
    }

    private void startPinging() {
        new Thread(() -> {
            while (running) {
                try {
                    if (out != null && pinging) {
                        out.println("PING");
                        System.out.println("Sent: PING");
                    }
                    Thread.sleep(5000); // Ожидание 5 секунд перед следующим пингом
                } catch (InterruptedException ex) {
                    System.err.println("Ping thread interrupted");
                    Thread.currentThread().interrupt(); // Сохраняем статус прерывания
                } catch (Exception ex) {
                    System.err.println("Failed to send PING");
                }
            }
        }).start();
    }

    public static void main(String[] args) {
        int ipValidation = -1;
        int portValidation = -1;
        do {
            ip = JOptionPane.showInputDialog(null,"Enter server ip");
            ipValidation = isValidIp();
            if(ipValidation == 0){
                JOptionPane.showMessageDialog(null, "Invalid ip input, please retry");
            }
        } while (ipValidation == 0);
        if (ipValidation == -1){
            System.exit(0);
        }


        do{
            port = JOptionPane.showInputDialog(null,"Enter server port");
            if(portValidation == 0) {
                JOptionPane.showMessageDialog(null, "Invalid port input, please retry");
            }
            portValidation = isValidPort();
        } while (portValidation == 0);
        if (portValidation == -1){
            System.exit(0);
        }

//        ip = JOptionPane.showInputDialog(null,"Enter server ip");
//        port = JOptionPane.showInputDialog(null,"Enter server port");
        SwingUtilities.invokeLater(() -> new BullsAndCowsClient(ip, port));
    }

    private static int isValidPort() {
        if (port == null){
            return -1;
        }
        if (port.trim().isEmpty()) {
            JOptionPane.showMessageDialog(null, "Port cannot be empty. Please try again.");
            return -1;
        }

        // Проверка, что порт — это число
        int intPort = Integer.parseInt(port);
        if (intPort <= 0 || intPort > 65535) {
            JOptionPane.showMessageDialog(null, "Port must be a number between 1 and 65535.");
            return -1;
        }
        return 1;
    }

    private static int isValidIp() {

        if (ip == null){
            return -1;
        }

        if (ip.trim().isEmpty()) {
            JOptionPane.showMessageDialog(null, "IP address cannot be empty. Please try again.");
            return 0;
        }

        // Проверка формата IP-адреса
//        if (!ip.matches("\\b(?:\\d{1,3}\\.){3}\\d{1,3}\\b")) {
//            JOptionPane.showMessageDialog(null, "Invalid IP address format. Please enter a valid IP.");
//            return false;
//        }
        return 1;
    }

    void addLogs(String s) {
        logs.add(s);
        gameLogArea.append(s);

    }

    void returnLogs(){
        for (int i = 0; i < logs.size();i++){
            gameLogArea.append(logs.get(i));
        }
    }

    void stopClient() {
        System.out.println("Stopping client...");

        // Останавливаем фоновую логику
        running = false;

        // Закрываем сокет и потоки
        try {
            if (socket != null && !socket.isClosed()) {
                socket.close();
//                System.out.println("Socket closed.");
            }
            if (in != null) {
                in.close();
//                System.out.println("Input stream closed.");
            }
            if (out != null) {
                out.close();
//                System.out.println("Output stream closed.");
            }
        } catch (IOException ex) {
            System.err.println("Error closing socket or streams");
        }

        // Останавливаем таймеры
        if (pingTimer != null && pingTimer.isRunning()) {
            pingTimer.stop();
//            System.out.println("Ping timer stopped.");
        }
        if (pingCheckTimer != null && pingCheckTimer.isRunning()) {
            pingCheckTimer.stop();
//            System.out.println("Ping check timer stopped.");
        }

        // Закрываем окна GUI
        SwingUtilities.invokeLater(() -> {
            if (gameFrame != null) {
                gameFrame.dispose();
                gameFrame = null;
//                System.out.println("Game frame closed.");
            }
            if (roomFrame != null) {
                roomFrame.dispose();
                roomFrame = null;
//                System.out.println("Room frame closed.");
            }
            if (roomSelectionFrame != null) {
                roomSelectionFrame.dispose();
                roomSelectionFrame = null;
//                System.out.println("Room selection frame closed.");
            }
        });

        // Завершаем серверный поток
        if (serverListener != null && serverListener.isAlive()) {
            try {
                serverListener.join(1000); // Ожидаем завершения потока (макс. 1 секунда)
                System.out.println("Server listener thread stopped.");
            } catch (InterruptedException ex) {
                System.err.println("Error stopping server listener");
                Thread.currentThread().interrupt(); // Сохраняем статус прерывания
            }
        }

    }

//    private void startPongTimeoutCheck() {
//        new Thread(() -> {
//            while (running) {
//                try {
//                    long currentTime = System.currentTimeMillis();
//                    if (currentTime - lastPongTime > 10_000) { // Проверяем, прошло ли больше 10 секунд
////                        serverListener.interrupt();
//                        int result = JOptionPane.showConfirmDialog(
//                                null,
//                                "Connection lost. Try to reconnect?",
//                                "Confirming dialog",
//                                JOptionPane.OK_CANCEL_OPTION,
//                                JOptionPane.WARNING_MESSAGE
//                        );
//                        try {
//                            if (result == JOptionPane.OK_OPTION) {
//                                out.println("NICKNAME:" + nickname);
//                                System.out.println("Sent nickname to server: " + nickname);
//                                String temp = in.readLine();
//                                if(temp.equals("CON_OK")){
//                                    lastPongTime = System.currentTimeMillis(); // Инициализация времени последнего PONG
//                                    running = true;
////                                    serverListener = new Thread(new ServerListener(this));
////                                    serverListener.start();
//                                    temp = in.readLine();
//                                    if(!temp.equals("NICKNAME_SET")){
//                                        stopClient();
//                                    }
//                                    player1Label = new JLabel("Player 1: ");
//                                    player2Label = new JLabel("Player 2: ");
//                                }else{
//                                    JOptionPane.showMessageDialog(this,"No response, try again or later");
//                                }
//                            } else {
//                                System.out.println("фыв"); // Действие при нажатии "Отмена" или закрытии окна
//                                stopClient();
//                            }
//
//                        }catch (Exception e){
//
//                        }
//
//                    }
//                    Thread.sleep(5000); // Проверяем каждые 5 секунд
//                } catch (InterruptedException ex) {
//                    System.err.println("Pong timeout check interrupted: " + ex.getMessage());
//                    Thread.currentThread().interrupt(); // Сохраняем статус прерывания
//                }
//            }
//        }).start();
//    }


}