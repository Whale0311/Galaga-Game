/*
 * app.cpp
 */
#include "app.hpp"
#include <cmsis_os2.h>
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "stack_macros.h"
#include "main.h"
#include <cstdlib> // Thư viện chứa hàm abs()

Game gameInstance;
Bullet shipBullet[MAX_BULLET];
Bullet enemyBullet[MAX_BULLET];
int enemyBulletType[MAX_BULLET];
Enemy enemy[MAX_ENEMY];

#define MAX_SPAWN_RATE 10000
int spawnRate = MAX_SPAWN_RATE;
int spawnSeed = 0;
bool isWaveInitialized = false;
int enemyMoveDirection = 1;

// CHUẨN 60FPS: Giảm xuống 90 frames (~1.5 giây đầu game quái mới bắt đầu nhả đạn)
int globalEnemyFireCooldown = 90;
extern RNG_HandleTypeDef hrng;
int currentRound = 1;
bool isRoundTransition = false;

// ĐIỀU CHỈNH TỐC ĐỘ ĐẠN: Giảm từ 5 xuống 4 để vừa tầm mắt né tránh
int enemyBulletSpeed = 4;
const int ENEMY_BULLET_SPEED_MAX = 12;

volatile bool isGameTaskTerminated = false;
uint16_t sx, sy;
bool shouldEndGame;
bool shouldStopTask;
uint32_t highScore = 0;

// Các biến toàn cục quản lý vật lý trái tim rơi
int heartDropX = -50;
int heartDropY = -50;
bool isHeartDropping = false;

#define FLASH_USER_SECTOR       FLASH_SECTOR_23
#define FLASH_USER_START_ADDR   0x081E0000

void SaveHighScoreToFlash(uint32_t score) {
    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError = 0;
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.Sector = FLASH_USER_SECTOR;
    EraseInitStruct.NbSectors = 1;

    __disable_irq();
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) == HAL_OK) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FLASH_USER_START_ADDR, score);
    }
    __enable_irq();
    HAL_FLASH_Lock();
}

void LoadHighScoreFromFlash() {
    highScore = *(__IO uint32_t*)FLASH_USER_START_ADDR;
    if (highScore == 0xFFFFFFFF || highScore > 99999) {
        highScore = 0;
    }
}

void updateEnemy(uint8_t dt);
void updateShipBullet(uint8_t dt);
void updateEnemyBullet(uint8_t dt);
void resetGameObjectsForNextRound();

uint8_t dt = 0;
extern osMessageQueueId_t Queue5Handle;

void gameTask(void *argument) {
    shouldEndGame = false;
    shouldStopTask = false;
    isWaveInitialized = false;
    enemyMoveDirection = 1;
    globalEnemyFireCooldown = 90; // CHUẨN 60FPS
    isHeartDropping = false;

    if (currentRound == 1) {
        gameInstance.ship.lives = 3;
        gameInstance.score = 0;
    }

	for (int i = 0; i < MAX_BULLET; i++) {
		shipBullet[i].updateCoordinate(-50, -50);
		enemyBullet[i].updateCoordinate(-50, -50);
		shipBullet[i].updateVelocity(0, -4); // Tốc độ đạn tàu bay lên vừa phải
		enemyBullet[i].updateVelocity(0, 4);  // Tốc độ đạn quái rơi xuống vừa phải
		shipBullet[i].updateDisplayStatus(IS_HIDDEN);
		enemyBullet[i].updateDisplayStatus(IS_HIDDEN);
	}

	for (int i = 0; i < MAX_ENEMY; i++) {
		enemy[i].updateCoordinate(-50, -50);
		enemy[i].updateDisplayStatus(IS_HIDDEN);
	}
	LoadHighScoreFromFlash();

	uint32_t tick = osKernelGetTickCount();
	for (;;) {
		tick += 16;

		if (shouldEndGame == true && shouldStopTask == true) {
			break;
		} else if (shouldEndGame) {
			osDelayUntil(tick);
			continue;
		}

		dt = 1; // Khóa cứng nhịp trễ vật lý ở 60 FPS

		sx = gameInstance.ship.coordinateX;
		sy = gameInstance.ship.coordinateY;

		updateEnemy(dt);
		gameInstance.ship.update(dt);
		updateShipBullet(dt);
		updateEnemyBullet(dt);

		/* KIỂM TRA VA CHẠM TÀU VÀ QUÁI */
		for (int i = 0; i < MAX_ENEMY; i++) {
            if (enemy[i].displayStatus != IS_SHOWN) continue;
            if (Entity::isCollide(enemy[i], gameInstance.ship)) {
                gameInstance.ship.updateShipHp(1);
                enemy[i].updateDisplayStatus(SHOULD_HIDE);
                gameInstance.ship.updateCoordinate(104, 260);

                HAL_GPIO_WritePin(GPIOG, GPIO_PIN_13, GPIO_PIN_SET);
                osDelay(150);
                HAL_GPIO_WritePin(GPIOG, GPIO_PIN_13, GPIO_PIN_RESET);
                break;
            }
		}

		/* KIỂM TRA VA CHẠM ĐẠN TÀU VÀ QUÁI */
		for (int i = 0; i < MAX_ENEMY; i++) {
			if (enemy[i].displayStatus != IS_SHOWN) continue;
			for (int j = 0; j < MAX_BULLET; j++) {
				if (shipBullet[j].displayStatus != IS_SHOWN) continue;
				if (Entity::isCollide(enemy[i], shipBullet[j])) {
					enemy[i].updateDisplayStatus(SHOULD_HIDE);
					shipBullet[j].updateDisplayStatus(SHOULD_HIDE);
					gameInstance.updateScore(5);

					// Tính năng rơi vật phẩm: 15% rơi ra trái tim
					if (!isHeartDropping && (HAL_GetTick() % 100 < 5)) {
					    heartDropX = enemy[i].coordinateX;
					    heartDropY = enemy[i].coordinateY;
					    isHeartDropping = true;
					}
					break;
				}
			}
		}

		/* CƠ CHẾ VẬT LÝ TRÁI TIM RƠI XUỐNG VÀ ĂN MẠNG */
				if (isHeartDropping) {
				    heartDropY += 2;

				    if (heartDropY > 320) {
				        isHeartDropping = false;
				        heartDropX = -100;
				        heartDropY = -100;
				    } else {
				        // THÊM ELSE VÀO ĐÂY: Chỉ kiểm tra va chạm khi trái tim vẫn đang rơi trên màn hình
				        int sX = gameInstance.ship.coordinateX;
				        int sY = gameInstance.ship.coordinateY;

				        if (heartDropX < sX + 32 && heartDropX + 32 > sX &&
				            heartDropY < sY + 32 && heartDropY + 32 > sY) {

				            if (gameInstance.ship.lives < 3) {
				                gameInstance.ship.lives++;

				                uint8_t msg = 3;
				                osMessageQueuePut(Queue5Handle, &msg, 0, 0);
				            }

				            isHeartDropping = false;
				            heartDropX = -100;
				            heartDropY = -100;
				        }
				    }
				}

		/* KIỂM TRA VA CHẠM ĐẠN QUÁI VÀ TÀU */
		for (int i = 0; i < MAX_BULLET; i++) {
			if (enemyBullet[i].displayStatus != IS_SHOWN) continue;
			if (Entity::isCollide(enemyBullet[i], gameInstance.ship)) {
                gameInstance.ship.updateShipHp(1);
                enemyBullet[i].updateDisplayStatus(SHOULD_HIDE);
                gameInstance.ship.updateCoordinate(104, 260);

                HAL_GPIO_WritePin(GPIOG, GPIO_PIN_13, GPIO_PIN_SET);
                osDelay(150);
                HAL_GPIO_WritePin(GPIOG, GPIO_PIN_13, GPIO_PIN_RESET);
                break;
			}
		}

		// Xử lý kết thúc game
		uint8_t msg;
		if (gameInstance.ship.lives <= 0) {
			shouldEndGame = true;
			if (gameInstance.score > highScore) {
				highScore = gameInstance.score;
				SaveHighScoreToFlash(highScore);
			}
			while (!shouldStopTask) {
				msg = 1;
				osMessageQueuePut(Queue5Handle, &msg, 0, 0);
				osDelay(50);
			}
			break;
		} 
		else if (isWaveInitialized) {
            int aliveEnemies = 0;
            for (int i = 0; i < MAX_ENEMY; i++) {
                if (enemy[i].displayStatus == IS_SHOWN || enemy[i].displayStatus == SHOULD_SHOW) {
                    aliveEnemies++;
                }
            }

            if (aliveEnemies == 0) {
                shouldEndGame = true;
                isRoundTransition = true;

                msg = 2;
                osMessageQueuePut(Queue5Handle, &msg, 0, 0);

                while (!shouldStopTask) {
                    osDelay(10);
                }
                break;
            }
        }
		// XÓA BỎ HOÀN TOÀN KHỐI LỆNH ELSE SPAM KÝ TỰ '0' ĐỂ GIẢI PHÓNG ĐƯỜNG TRUYỀN QUEUE
		osDelayUntil(tick);
	}
	isGameTaskTerminated = true;
	return;
}

void updateEnemy(uint8_t dt) {
	int effectiveRound = (currentRound > 5) ? 5 : currentRound;
	static int dropTimer = 0;

	if (!isWaveInitialized) {
		int numEnemies = effectiveRound * 6;
		if (numEnemies > MAX_ENEMY) numEnemies = MAX_ENEMY;

		int cols = 6;
		int startSpeed = (currentRound >= 3) ? 2 : 1;
		enemyMoveDirection = 1;

		for (int i = 0; i < numEnemies && i < MAX_ENEMY; i++) {
			int row = i / cols;
			int col = i % cols;
			uint16_t startX = 10 + col * 35;
			uint16_t startY = 20 + row * 30;

			enemy[i].updateCoordinate(startX, startY);
			enemy[i].updateVelocity(enemyMoveDirection * startSpeed, 0);
			enemy[i].updateDisplayStatus(SHOULD_SHOW);
		}
		isWaveInitialized = true;
		dropTimer = 0;
	}

	for (int i = 0; i < MAX_ENEMY; i++) {
		if (enemy[i].displayStatus == IS_SHOWN || enemy[i].displayStatus == SHOULD_SHOW) {
			enemy[i].update(dt);
			if (enemy[i].coordinateX >= 240) {
				enemy[i].updateCoordinate(-32, enemy[i].coordinateY);
			}
		}
	}

	dropTimer++;
	int dropThreshold = 200 - (effectiveRound * 20);
	if (dropThreshold < 80) dropThreshold = 80;

	if (dropTimer >= dropThreshold) {
		dropTimer = 0;
		for (int i = 0; i < MAX_ENEMY; i++) {
			if (enemy[i].displayStatus == IS_SHOWN || enemy[i].displayStatus == SHOULD_SHOW) {
				enemy[i].updateCoordinate(enemy[i].coordinateX, enemy[i].coordinateY + 15);
				if (enemy[i].coordinateY > 280) {
					 enemy[i].updateCoordinate(enemy[i].coordinateX, -32);
				}
			}
		}
	}
}

void updateShipBullet(uint8_t dt) {
	bool shouldFire = gameInstance.ship.updateBullet(dt);
	for (int i = 0; i < MAX_BULLET; i++) {
		if (shipBullet[i].displayStatus != IS_HIDDEN) {
			shipBullet[i].update(dt);
			continue;
		}

		if (shouldFire) {
			shipBullet[i].updateCoordinate(sx, sy - 16);
			shipBullet[i].updateDisplayStatus(SHOULD_SHOW);
			shouldFire = false;
		}
	}
}

void updateEnemyBullet(uint8_t dt) {
	int bulletSpeed = enemyBulletSpeed;
	globalEnemyFireCooldown -= dt;

	if (globalEnemyFireCooldown <= 0) {
		int activeEnemies[MAX_ENEMY];
		int activeCount = 0;

		for (int j = 0; j < MAX_ENEMY; j++) {
			if (enemy[j].displayStatus == IS_SHOWN) {
				activeEnemies[activeCount] = j;
				activeCount++;
			}
		}

		if (activeCount > 0) {
			uint32_t randValue = 0;
			if (HAL_RNG_GenerateRandomNumber(&hrng, &randValue) != HAL_OK) {
				randValue = 0;
			}

			int shooterIndex = activeEnemies[randValue % activeCount];

			for (int i = 0; i < MAX_BULLET; i++) {
				if (enemyBullet[i].displayStatus == IS_HIDDEN) {
					enemyBullet[i].updateCoordinate(enemy[shooterIndex].coordinateX, enemy[shooterIndex].coordinateY + 16);
					enemyBullet[i].updateVelocity(0, bulletSpeed);
					enemyBullet[i].updateDisplayStatus(SHOULD_SHOW);
					enemyBulletType[i] = shooterIndex % 3;
					break;
				}
			}

			int effectiveRound = (currentRound > 5) ? 5 : currentRound;

            // CHUẨN ĐẾM THEO KHUNG HÌNH (60FPS): Tần suất quái xả đạn lý tưởng
            int baseRate = 60 - (effectiveRound * 10);
            if (baseRate < 20) baseRate = 20;

            globalEnemyFireCooldown = baseRate + (randValue % 30);
		}
	}

	for (int i = 0; i < MAX_BULLET; i++) {
		if (enemyBullet[i].displayStatus != IS_SHOWN)
			continue;
		enemyBullet[i].update(dt);
	}
}

void resetGameObjectsForNextRound() {
    for (int i = 0; i < MAX_ENEMY; i++) {
        enemy[i].updateCoordinate(-50, -50);
        enemy[i].updateDisplayStatus(IS_HIDDEN);
    }
    for (int i = 0; i < MAX_BULLET; i++) {
        shipBullet[i].updateCoordinate(-50, -50);
        shipBullet[i].updateDisplayStatus(IS_HIDDEN);
        enemyBullet[i].updateCoordinate(-50, -50);
        enemyBullet[i].updateDisplayStatus(IS_HIDDEN);
    }
    gameInstance.ship.coordinateX = 104;
    gameInstance.ship.coordinateY = 260;
    gameInstance.ship.updateVelocityX(0);
    gameInstance.ship.updateVelocityY(0);
    spawnRate = 0;
    shouldEndGame = false;
    shouldStopTask = false;
    isWaveInitialized = false;
    enemyMoveDirection = 1;
    isHeartDropping = false;
    globalEnemyFireCooldown = 90; // Sửa về mốc frame ngắn chuẩn 60FPS
}
