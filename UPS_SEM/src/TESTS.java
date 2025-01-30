import javax.swing.*;

public class TESTS extends JFrame{
    static String nickname;
    TESTS(){
        while (nickname == null || nickname.trim().isEmpty()) {
            nickname = JOptionPane.showInputDialog(this, "Enter your nickname:");

            if (nickname == null) {
                // Пользователь закрыл диалоговое окно, можно выйти из цикла
                JOptionPane.showMessageDialog(this, "Nickname cannot be empty. Please try again.");
            } else if (nickname.trim().isEmpty()) {
                JOptionPane.showMessageDialog(this, "Nickname cannot be empty or contain only spaces. Please try again.");
            }
            System.out.println("aaa");
        }
    }

    public static void main(String[] args) {
        TESTS A = new TESTS();
    }


}
