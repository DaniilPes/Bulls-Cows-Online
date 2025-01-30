import javax.swing.*;
import java.io.*;
import java.net.Socket;
import java.util.ArrayList;

class ServerListener implements Runnable {
    private BullsAndCowsClient client;
    Timer timeoutTimer;
    private boolean roomFail = false;
    int badRequestCount = 0;
    ServerListener(BullsAndCowsClient client){
        this.client = client;
    }

    @Override
    public void run() {
        client.fetchRooms();
        String response = null;
        while (client.running) {
            try {
                response = client.in.readLine();
            } catch (IOException e) {
                System.out.println("Can't read the line");
//                    throw new RuntimeException(e);
                response = "-";
            }

            System.out.println("RESPONSE: =" + response + "=");

            if (response != null) {
                if (response.equals("-") && !client.clup) { //123
                    System.out.println("Server closed the connection or no data received.");
                    boolean reconnected = attemptReconnection(180_000); // 60 секунд
                    if (!reconnected) {
                        System.out.println("Reconnection failed. Exiting...");
                        client.cleanupAndExit();
                        break;
                    } else {
//                            returnToMain();
                        System.out.println("SVD_SVD_SVD" + client.savedRoom);
                        client.out.println("JOIN_ROOM:" + client.savedRoom);
                        client.gameLogArea.setText("");
                        client.isReconnect = false;
                        client.isOppReconnected = false;
                        System.out.println("fetching rooms");
                        client.guessButton.setEnabled(false);

                        System.out.println("returning to main");
                        response = "PONG";
                    }
                }
            }else {
                System.out.println("SERVER_CLOSED_DETECTED");
                client.stopClient();
                client.cleanupAndExit();
            }

            response = response.trim();
            if ("PONG".equals(response)) {
                System.out.println("`````````````````````````PONG`````````````````````````");
                client.lastPongTime = System.currentTimeMillis();
            }else{
                if(!response.equals("")){
                    if (client.running && client.gameLogArea != null) {
                        System.out.println("Connection lost.");
                    }
                    System.out.println("Received from server: " + response);
                    try {
                        processRequest(response);
                    } catch (IOException e) {
                        System.out.println("Stopping client 1228");
//                            throw new RuntimeException(e);
                    }
                }

            }
        }

    }

    private void returnToMain() {
//        client.gameLogArea.setText("");
        if(client.roomSelectionFrame != null){
            client.roomSelectionFrame.dispose();
        }
        if(client.roomFrame != null){
            client.roomFrame.dispose();
        }
        if(client.gameFrame != null){
            client.gameFrame.dispose();
        }
        client.gameLogArea.setText("");
        client.isReconnect = false;
        client.isOppReconnected = false;
        System.out.println("fetching rooms");
        client.guessButton.setEnabled(false);
        client.isRoomSelectionOpen = false;
        client.fetchRooms();
    }

    private void processRequest(String response) throws IOException {
        if(response.equals("U_C")){
            badRequestCount++;
            if(badRequestCount >= 3){
                client.out.println("LEAVE");
                System.out.println("3 bad requests - disconnecting");
                client.cleanupAndExit();
            }
        }else{
            badRequestCount = 0;
        }
        if (response.startsWith("OPPONENT_LEFT:")) {
            client.addLogs("Your opponent has left the game. Waiting for reconnection...\n");
            timeoutTimer = new Timer(30_000, e -> {
                if (client.running && !client.isOppReconnected && !client.leave) {
                    client.out.println("LEAVE_GAME");
                    client.leave = true;
                    JOptionPane.showMessageDialog(client.roomSelectionFrame,"Opponent's reconnection time expired... leaving the game...");
//                    client.guessButton.setEnabled(false);
                    client.returnToRoomSelection();
                    roomFail = false;
                }
            });
            timeoutTimer.setRepeats(false); // Срабатывает только один раз
            timeoutTimer.start();

        } else if (response.startsWith("GAME_ENDED")) {
            client.addLogs("Game ended. Opponent did not reconnect.\n");
            JOptionPane.showMessageDialog(client.gameFrame, "Game ended. Opponent did not reconnect.");
            client.returnToRoomSelection(); // Вернуться в меню выбора комнат
        }else if (response.equals("RECONNECT")){
            client.isReconnect = true;
        }else if(response.equals("OPPONENT_RECONNECTED")){
            System.out.println("===== opponent_reconnected ======");
            client.isOppReconnected = true;
            client.addLogs("Opponent reconnected\n");
        }else if (response.equals("===LOOSER===") || response.equals("===WINNER===")) {
            JOptionPane.showMessageDialog(client.roomFrame, response);
            client.returnToRoomSelection();
            client.isReconnect = false;
            client.gameLogArea.setText("");
            client.logs = new ArrayList<>();
            client.isOppReconnected = false;
        }else if(response.equals("LEAVE")){
            System.out.println("Leaving in progress...");
        }else if (response.startsWith("RECONNECTED")) {
            client.returnLogs();
            client.gameLogArea.append("Reconnected successfully! Resuming the game...\n");
            client.isReconnect = true;
//            if (!client.isGameStarted) {
//                client.initializeGameWindow();
//            }
        }else if (response.equals("GAME_STARTED")) {
            SwingUtilities.invokeLater(() -> {
                client.isGameStarted = true;
                System.out.println("Game started! Good luck!\n");
                try {
                    client.initializeGameWindow(); // Открываем игровое окно
                } catch (IOException e) {
                    throw new RuntimeException(e);
                }
            });
        } else if (response.startsWith("RESULT:")) {
            String res = response.substring("RESULT:".length());
            client.addLogs(res + "\n");
        } else if (response.startsWith("ROOM_LIST:")) {
            client.roomsData = response.substring("ROOM_LIST:".length());
            String[] roomArray = client.roomsData.split(",");
            client.validateSavedRoom(roomArray);

            if (client.isRoomSelectionOpen && client.roomList != null) {
                client.updateRoomList(roomArray);
            } else if (!client.isRoomSelectionOpen) {
                SwingUtilities.invokeLater(() -> client.showRoomSelectionWindow(roomArray));
            }
        } else if (response.startsWith("PLAYER_COUNT:")) {
            client.playerCount = Integer.parseInt(response.split(":")[1].trim());
            client.playerCountLabel.setText("Players in room: " + client.playerCount);
        } else if (response.startsWith("NICKNAMES:")) {
            System.out.println("----------------" + response + "---------------");
            String[] nicknames = response.substring("NICKNAMES:".length()).split(",");
            client.updatePlayerLabels(nicknames);
        } else if (response.startsWith("UNLOCK_GUESS")) {
            client.guessButton.setEnabled(true);
            client.guess_unlock = true;
        }else if(response.equals("ROOM_FAIL")){
            client.connectionError = true;
        } else if (response.equals("ROOM_CLEARED")) {
            if(timeoutTimer != null){
                if(timeoutTimer.isRunning()){
                    timeoutTimer.stop();
                }
            }

        } else if(response.equals("")){
//            System.out.println();
        }else{
            System.out.println("UnknownCommand: -" + response + "-");
        }
    }

    private boolean attemptReconnection(long timeoutMillis) {
        client.gameLogArea.append("attempting reconnect...");
        long startTime = System.currentTimeMillis();
        while (System.currentTimeMillis() - startTime < timeoutMillis) {
            try {
                // Закрываем старый сокет, если он открыт
                if (client.socket != null && !client.socket.isClosed()) {
                    client.socket.close();
                }

                // Пытаемся переподключиться
                client.socket = new Socket(client.ip, Integer.parseInt(client.port)); // Укажите адрес и порт сервера
                client.out = new PrintWriter(client.socket.getOutputStream(), true);
                client.in = new BufferedReader(new InputStreamReader(client.socket.getInputStream()));

//                 Отправляем никнейм заново
                client.out.println("NICKNAME:" + client.nickname);
                String response = client.in.readLine();
                if ("CON_OK".equals(response)) {
                    System.out.println("!RECONNECTED!");
                    client.lastPongTime = System.currentTimeMillis();
                    response = client.in.readLine();
                    if(response.equals("NICKNAME_SET")){
                        System.out.println("nickname approved");
                    }

//                    client.fetchRooms();
                    return true; // Успешное подключение
                }
            } catch (IOException ex) {
                System.out.println("Reconnection attempt failed. Retrying...");
            }

            // Задержка перед следующей попыткой
            try {
                Thread.sleep(5000); // Ожидание 1 секунды перед повторной попыткой
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                return false; // Прерывание попыток переподключения
            }
        }
        return false; // Не удалось подключиться в течение таймаута
    }


}
