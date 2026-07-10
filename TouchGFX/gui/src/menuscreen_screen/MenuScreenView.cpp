#include <gui/menuscreen_screen/MenuScreenView.hpp>
#include "app.hpp"
// THÊM CÁC DÒNG NÀY VÀO ĐỂ KHAI BÁO VỚI TRÌNH BIÊN DỊCH
extern uint32_t highScore;
extern void LoadHighScoreFromFlash();
MenuScreenView::MenuScreenView()
{

}

void MenuScreenView::setupScreen()
{
    MenuScreenViewBase::setupScreen();

    // Bây giờ nó sẽ nhận diện được hàm này
    LoadHighScoreFromFlash();

    // Bây giờ nó sẽ nhận diện được biến này
    Unicode::snprintf(menu_highscoreBuffer, MENU_HIGHSCORE_SIZE, "%d", highScore);

    menu_highscore.invalidate();
}

void MenuScreenView::tearDownScreen()
{
    MenuScreenViewBase::tearDownScreen();
}
