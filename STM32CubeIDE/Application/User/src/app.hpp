/*
 * app.hpp
 */

#ifndef APPLICATION_USER_SRC_APP_HPP_
#define APPLICATION_USER_SRC_APP_HPP_
#include "Game.hpp"
#include "Entity.hpp"

extern Game gameInstance;
extern bool shouldEndGame;
extern bool shouldStopTask;
extern Enemy enemy[MAX_ENEMY];
extern Bullet shipBullet[MAX_BULLET];
extern Bullet enemyBullet[MAX_BULLET];
extern int enemyBulletType[MAX_BULLET];
extern void gameTask(void *argument);
extern uint32_t highScore;
extern void LoadHighScoreFromFlash();
#endif /* APPLICATION_USER_SRC_APP_HPP_ */
