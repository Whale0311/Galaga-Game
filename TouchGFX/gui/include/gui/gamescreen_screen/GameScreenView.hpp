#ifndef GAMESCREENVIEW_HPP
#define GAMESCREENVIEW_HPP

#include <gui_generated/gamescreen_screen/GameScreenViewBase.hpp>
#include <gui/gamescreen_screen/GameScreenPresenter.hpp>
#include "app.hpp"

class GameScreenView : public GameScreenViewBase
{
public:
    GameScreenView();
    virtual ~GameScreenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    void handleTickEvent();

protected:
    void getInput();
    void bringUIElementsToFront();

    // CÁC BIẾN ĐẾM THỜI GIAN PHẢI NẰM Ở ĐÂY (TRƯỚC DẤU };)
    int transitionDelayCounter;
    int restartDelayCounter;

private:
    touchgfx::Image shipImage;
    touchgfx::AnimatedImage enemyImage[MAX_ENEMY];
    touchgfx::Image shipBulletImage[MAX_BULLET];
    touchgfx::Image enemyBulletImage[MAX_BULLET];
    touchgfx::Image backgroundImage;
    touchgfx::Image fallingHeart;
}; // Dấu kết thúc Class phải nằm sau cùng

#endif // GAMESCREENVIEW_HPP
