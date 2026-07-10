#include <gui/gamescreen_screen/GameScreenView.hpp>

#include "app.hpp"
#include <BitmapDatabase.hpp>
#include <cmsis_os2.h>
#include <cmsis_os.h>
#include <cstring>

// Khai báo biến extern và một số biến toàn cục khác
extern void gameTask(void *argument);

extern int currentRound;
extern bool isRoundTransition;
extern int enemyBulletSpeed;
extern const int ENEMY_BULLET_SPEED_MAX;
extern void resetGameObjectsForNextRound();
extern int spawnRate;

extern const int MAX_SPAWN_RATE;


osThreadId_t gameTaskHandle;
uint8_t hearts = 3;
bool shouldStopScreen;
extern osMessageQueueId_t Queue1Handle;
//extern osMessageQueueId_t Queue2Handle;
extern osMessageQueueId_t Queue3Handle;
extern osMessageQueueId_t Queue4Handle;
extern osMessageQueueId_t Queue5Handle;

GameScreenView::GameScreenView() {
	transitionDelayCounter = 0;
	restartDelayCounter = 0;
	// Khởi tạo đối tượng Game và các thành phần đồ họa trên màn hình game
	gameInstance = Game();
	remove(menu_button);
	// menu_button.setVisible(false);
	remove(score_holder);
	remove(highscore_holder);
	remove(round_2);
	remove(image2);
	fallingHeart.setBitmap(touchgfx::Bitmap(BITMAP_HEART_ID));
	fallingHeart.setVisible(false);
	add(fallingHeart);
	// Prepare ship
	// Chuẩn bị hình ảnh cho tàu và thiết lập vị trí ban đầu
	backgroundImage.setBitmap(Bitmap(BITMAP_ALTERNATE_THEME_IMAGES_BACKGROUNDS_240X320_PUZZLE_ID));
	backgroundImage.setXY(0, 0);
	backgroundImage.setVisible(false);
	add(backgroundImage);

	shipImage.setBitmap(touchgfx::Bitmap(BITMAP_SHIP_MAIN_ID));
	shipImage.setXY(gameInstance.ship.coordinateX,
			gameInstance.ship.coordinateY);
	add(shipImage);
	// Chuẩn bị hình ảnh cho đạn
	for (int i = 0; i < MAX_BULLET; i++) {
		enemyBulletImage[i].setXY(321, 33);
		enemyBulletImage[i].setBitmap(touchgfx::Bitmap(BITMAP_ENEMY_BULLET_RED_ID)); // Cứ set mặc định là Đỏ
		shipBulletImage[i].setXY(321, 33);
		shipBulletImage[i].setBitmap(touchgfx::Bitmap(BITMAP_BULLET_DOUBLE_ID));
	}

	// Chuẩn bị hình ảnh mặt người cho kẻ địch
	for (int i = 0; i < MAX_ENEMY; i++) {
		switch (i % 3) {
		case 0:
			// Truyền cùng 1 ID để ảnh không bị nhấp nháy
			enemyImage[i].setBitmaps(BITMAP_MRTIEN_ID, BITMAP_MRTIEN_ID);
			break;
		case 1:
			enemyImage[i].setBitmaps(BITMAP_MRTUNG_ID, BITMAP_MRTUNG_ID);
			break;
		case 2:
			enemyImage[i].setBitmaps(BITMAP_MRTIEN2_ID, BITMAP_MRTIEN2_ID);
			break;
		default:
			break;
		}
		// Xóa dòng enemyImage[i].setUpdateTicksInterval(20); đi vì không cần animation nữa
	}
}

void GameScreenView::setupScreen() {
	GameScreenViewBase::setupScreen();

		backgroundImage.setVisible(true);
		bringUIElementsToFront();
	// Kết thúc task game trước khi bắt đầu màn hình game mới (đảm bảo không có task game nào đang chạy)
	osThreadTerminate(gameTaskHandle);
	osMessageQueueReset(Queue5Handle);
	const osThreadAttr_t gameTask_attributes = { .name = "gameTask", // Tên của task là "gameTask"
			.stack_size = 8192 * 2, // Kích thước của stack được cấp phát cho task là 8192 * 2 bytes
			.priority = (osPriority_t) osPriorityNormal, // Ưu tiên của task được đặt là osPriorityNormal (ưu tiên bình thường)
			};

	// Tạo một task mới có các thuộc tính đã được khai báo trước đó
	gameTaskHandle = osThreadNew(gameTask, NULL, &gameTask_attributes);
	shouldEndGame = false;
	shouldStopTask = false;
	shouldStopScreen = false;
}

void GameScreenView::tearDownScreen()
{
    GameScreenViewBase::tearDownScreen();
    
    // Đảm bảo dừng task game hoàn toàn
    if (gameTaskHandle != nullptr) {
        osThreadTerminate(gameTaskHandle);
        gameTaskHandle = nullptr;
    }
    
    // Reset các biến quan trọng
    currentRound = 1;
    isRoundTransition = false;
    shouldEndGame = false;
    shouldStopTask = false;
    shouldStopScreen = false;
    
    // Reset trạng thái game
    gameInstance.reset(); // Thêm hàm reset() vào class Game nếu chưa có
}

// void GameScreenView::handleKeyEvent(uint8_t key)
// {
//     if (key == 'B') { // GO BACK button
//         // Dừng task game nếu đang chạy
//         if (gameTaskHandle != nullptr) {
//             osThreadTerminate(gameTaskHandle);
//             gameTaskHandle = nullptr;
//         }
        
//         // Reset các biến trạng thái
//         currentRound = 1;
//         isRoundTransition = false;
//         shouldEndGame = false;
//         shouldStopTask = false;
//         shouldStopScreen = false;
        
//         // Chuyển về màn hình chính
//         static_cast<FrontendApplication*>(Application::getInstance())->gotoMainMenuScreen();
//     }
    
//     // Gọi hàm base để xử lý các key event khác
//     GameScreenViewBase::handleKeyEvent(key);
// }

void GameScreenView::bringUIElementsToFront() {
    // Đưa các thành phần UI quan trọng lên trên cùng
    remove(score_board);
    add(score_board);
    
    remove(heart_01);
    add(heart_01);
    
    remove(heart_02);
    add(heart_02);
    
    remove(heart_03);
    add(heart_03);
    // KÉO TRÁI TIM RƠI LÊN LỚP HIỂN THỊ TRÊN CÙNG
	remove(fallingHeart);
	add(fallingHeart);
    // Invalidate để vẽ lại
    score_board.invalidate();
    heart_01.invalidate();
    heart_02.invalidate();
    heart_03.invalidate();
}

// Render game objects
void GameScreenView::handleTickEvent() {
	GameScreenViewBase::handleTickEvent();
	extern int heartDropX;
	extern int heartDropY;
	extern bool isHeartDropping;

	if (isHeartDropping) {
		    fallingHeart.setVisible(true);
		    fallingHeart.moveTo(heartDropX, heartDropY);
		} else if (fallingHeart.isVisible()) {
		    fallingHeart.setVisible(false);
		    fallingHeart.invalidate();
	}
	// display end game screen
	uint8_t stopFlag=0;
	uint32_t count5 = osMessageQueueGetCount(Queue5Handle);
	// get latest message
	while (count5 > 0) {
		osMessageQueueGet(Queue5Handle, &stopFlag, NULL, 0);
		count5 --;
	}
	if (stopFlag == 1 && !shouldStopScreen) {
			// Thêm tấm nền xanh (image2) và các đối tượng vào màn hình
			add(image2);
			add(menu_button);
			add(score_holder);
			add(highscore_holder);

			// Ép chúng phải hiện hình (đè lên cài đặt Visible cũ nếu có)
			image2.setVisible(true);
			menu_button.setVisible(true);
			score_holder.setVisible(true);
			highscore_holder.setVisible(true);

			Unicode::snprintf(score_holderBuffer, SCORE_HOLDER_SIZE, "%d", gameInstance.score);
			Unicode::snprintf(highscore_holderBuffer, HIGHSCORE_HOLDER_SIZE, "%d", highScore);

			// Cập nhật đồ họa
			image2.invalidate();
			menu_button.invalidate();
			score_holder.invalidate();
			highscore_holder.invalidate();
			invalidate();

			shouldStopTask = true;
			shouldStopScreen = true;
			if (gameTaskHandle != nullptr) {
						osThreadTerminate(gameTaskHandle);
						gameTaskHandle = nullptr;
					}
		}

	if (stopFlag == 2 && !shouldStopScreen) {
			add(round_2);

			// THÊM DÒNG NÀY: Truyền số Round tiếp theo vào chữ ROUND <value>
			// Vì biến currentRound sẽ tăng sau 3 giây chờ, nên lúc này ta in currentRound + 1
			Unicode::snprintf(round_2Buffer, ROUND_2_SIZE, "%d", currentRound + 1);

			round_2.invalidate();
			invalidate();

			shouldStopTask = true;
			shouldStopScreen = true;
			if (gameTaskHandle != nullptr) {
						osThreadTerminate(gameTaskHandle);
						gameTaskHandle = nullptr; // <--- THÊM DÒNG NÀY VÀO ĐÂY NỮA
					}

					transitionDelayCounter = 180;
		}
	if (stopFlag == 3 && !shouldStopScreen) {
	    hearts = gameInstance.ship.lives; // Cập nhật biến mạng
	    // Sáng lại các trái tim tùy theo số mạng
	    if (hearts >= 1) heart_03.setAlpha(255);
	    if (hearts >= 2) heart_02.setAlpha(255);
	    if (hearts == 3) heart_01.setAlpha(255);

	    heart_01.invalidate();
	    heart_02.invalidate();
	    heart_03.invalidate();
	}

	// Get input
	uint8_t res = 0;

	uint32_t count = osMessageQueueGetCount(Queue1Handle);
	if (count > 0) {
		osMessageQueueGet(Queue1Handle, &res, NULL, osWaitForever);
		if (res == 'R') {
			gameInstance.ship.updateVelocityX(gameInstance.ship.VELOCITY);
			shipImage.setBitmap(touchgfx::Bitmap(BITMAP_SHIP_RIGHT_ID));
			osMessageQueueReset(Queue1Handle);
		} else if (res == 'N') {
			gameInstance.ship.updateVelocityX(0);
			shipImage.setBitmap(touchgfx::Bitmap(BITMAP_SHIP_MAIN_ID));
		} else {
			gameInstance.ship.updateVelocityX(-gameInstance.ship.VELOCITY);
			shipImage.setBitmap(touchgfx::Bitmap(BITMAP_SHIP_LEFT_ID));
			osMessageQueueReset(Queue1Handle);
		}
	}

	uint32_t count3 = osMessageQueueGetCount(Queue3Handle);
	if (count3 > 0) {
		osMessageQueueGet(Queue3Handle, &res, NULL, osWaitForever);
		if (res == 'U') {
			gameInstance.ship.updateVelocityY(gameInstance.ship.VELOCITY);
			osMessageQueueReset(Queue4Handle);
		} else if (res == 'N') {
			gameInstance.ship.updateVelocityY(0);
		}
	}

	uint32_t count4 = osMessageQueueGetCount(Queue4Handle);
	if (count4 > 0) {
		osMessageQueueGet(Queue4Handle, &res, NULL, osWaitForever);
		if (res == 'D') {
			gameInstance.ship.updateVelocityY(-gameInstance.ship.VELOCITY);
			osMessageQueueReset(Queue3Handle);
		} else if (res == 'N') {
			gameInstance.ship.updateVelocityY(0);
		}
	}

	// update player position and sprite
	shipImage.moveTo(gameInstance.ship.coordinateX,
			gameInstance.ship.coordinateY);

	// render ship bullet
	for (int i = 0; i < MAX_BULLET; i++) {
		switch (shipBullet[i].displayStatus) {
		case IS_HIDDEN:
			break;
		case IS_SHOWN:
			shipBulletImage[i].moveTo(shipBullet[i].coordinateX,
					shipBullet[i].coordinateY);
			break;
		case SHOULD_SHOW:
			shipBulletImage[i].moveTo(shipBullet[i].coordinateX,
					shipBullet[i].coordinateY);
			add(shipBulletImage[i]);
			shipBullet[i].updateDisplayStatus(IS_SHOWN);
			break;
		case SHOULD_HIDE:
			remove(shipBulletImage[i]);
			shipBullet[i].updateDisplayStatus(IS_HIDDEN);
			break;
		default:
			break;
		}
	}

	// render enemy bullet
		for (int i = 0; i < MAX_BULLET; i++) {
			switch (enemyBullet[i].displayStatus) {
			case IS_HIDDEN:
				break;
			case IS_SHOWN:
				enemyBulletImage[i].moveTo(enemyBullet[i].coordinateX,
						enemyBullet[i].coordinateY);
				break;
			case SHOULD_SHOW:
				enemyBulletImage[i].moveTo(enemyBullet[i].coordinateX,
						enemyBullet[i].coordinateY);

				// THÊM ĐOẠN NÀY: Đổi ảnh đạn tương ứng với chủ nhân của nó
				if (enemyBulletType[i] == 0) {
					enemyBulletImage[i].setBitmap(touchgfx::Bitmap(BITMAP_ENEMY_BULLET_GREEN_ID)); // Đạn của mrTien
				} else if (enemyBulletType[i] == 1) {
					enemyBulletImage[i].setBitmap(touchgfx::Bitmap(BITMAP_ENEMY_BULLET_RED_ID));   // Đạn của mrTung
				} else {
					enemyBulletImage[i].setBitmap(touchgfx::Bitmap(BITMAP_ENEMY_BULLET_YELLOW_ID)); // Đạn của mrTien2
				}

				add(enemyBulletImage[i]);
				enemyBullet[i].updateDisplayStatus(IS_SHOWN);
				break;
			case SHOULD_HIDE:
				remove(enemyBulletImage[i]);
				enemyBullet[i].updateDisplayStatus(IS_HIDDEN);
				break;
			default:
				break;
			}
		}

	// render enemy
	for (int i = 0; i < MAX_ENEMY; i++) {
		switch (enemy[i].displayStatus) {
		case IS_SHOWN:
			enemyImage[i].moveTo(enemy[i].coordinateX, enemy[i].coordinateY);
			break;
		case SHOULD_SHOW:
			enemyImage[i].moveTo(enemy[i].coordinateX, enemy[i].coordinateY);
			//enemyImage[i].startAnimation(false, true, true);
			add(enemyImage[i]);
			enemy[i].updateDisplayStatus(IS_SHOWN);
			break;
		case SHOULD_HIDE:
			//enemyImage[i].stopAnimation();
			remove(enemyImage[i]);
			enemy[i].updateDisplayStatus(IS_HIDDEN);
			break;
		case IS_HIDDEN:
		default:
			break;
		}
	}

	// check if player is out of health and should end game
	if (gameInstance.ship.lives != hearts) {
		hearts = gameInstance.ship.lives;
		// reset hearts
		heart_03.setAlpha(80);
		heart_02.setAlpha(80);
		heart_01.setAlpha(80);
		if (hearts >= 1)
			heart_03.setAlpha(255);
		if (hearts >= 2)
			heart_02.setAlpha(255);
		if (hearts >= 3)
			heart_01.setAlpha(255);
		// If player is out of health
		if (hearts < 1) {
			invalidate();
		}
	}

	Unicode::snprintf(score_boardBuffer, SCORE_BOARD_SIZE, "%d",
			gameInstance.score);
	invalidate();
	

	// ==========================================================
		// 1. KHỐI TỰ CHUYỂN MÀN (Khi bắn hết quái, stopFlag == 2)
		// (Đây chính là đoạn code bạn vừa gửi)
		// ==========================================================
		if (transitionDelayCounter > 0) {
			transitionDelayCounter--;
			if (transitionDelayCounter == 0) {
				// Đảm bảo dừng task cũ hoàn toàn
				if (gameTaskHandle != nullptr) {
					osThreadTerminate(gameTaskHandle);
					gameTaskHandle = nullptr;
				}
				currentRound++; // Tăng màn lên
				// Khi chuẩn bị vào màn mới (currentRound đã tăng)
				switch (currentRound) {
				    case 1:
				        backgroundImage.setBitmap(touchgfx::Bitmap(BITMAP_BG_ROUND1_ID));
				        break;
				    case 2:
				        backgroundImage.setBitmap(touchgfx::Bitmap(BITMAP_BG_ROUND2_ID));
				        break;
				    case 3:
						backgroundImage.setBitmap(touchgfx::Bitmap(BITMAP_BG_ROUND3_ID));
						break;
					case 4:
						backgroundImage.setBitmap(touchgfx::Bitmap(BITMAP_BG_ROUND4_ID));
						break;
				    default:
				        backgroundImage.setBitmap(touchgfx::Bitmap(BITMAP_BG_ROUND5_ID));
				        break;
				}
				invalidate();
				backgroundImage.invalidate();
				isRoundTransition = false;
				shouldEndGame = false;
				shouldStopTask = false;
				shouldStopScreen = false;

				backgroundImage.setVisible(true);
				backgroundImage.invalidate();
				osDelay(100);

				// Reset trạng thái game cho round mới (KHÔNG reset điểm và mạng)
				bringUIElementsToFront();
				resetGameObjectsForNextRound();

				for (int i = 0; i < MAX_ENEMY; i++) {
					remove(enemyImage[i]);
				}
				for (int i = 0; i < MAX_BULLET; i++) {
					remove(shipBulletImage[i]);
					remove(enemyBulletImage[i]);
				}

				// Ẩn UI chuyển màn
				remove(round_2);
				round_2.invalidate();
				invalidate();

				// Vẽ lại tim và điểm
				heart_01.invalidate();
				heart_02.invalidate();
				heart_03.invalidate();
				score_board.invalidate();

				stopFlag = 0;
				spawnRate = 0;

				// Tạo lại task
				const osThreadAttr_t gameTask_attributes = {
					.name = "gameTask",
					.stack_size = 8192 * 2,
					.priority = (osPriority_t) osPriorityNormal
				};
				gameTaskHandle = osThreadNew(gameTask, NULL, &gameTask_attributes);
			}
		}

		if (shouldStopScreen && menu_button.isVisible()) {
								if (menu_button.getPressedState()) {
									// 1. Dọn dẹp luồng và hàng đợi tín hiệu cũ
									if (gameTaskHandle != nullptr) {
										osThreadTerminate(gameTaskHandle);
										gameTaskHandle = nullptr;
									}
									osMessageQueueReset(Queue5Handle);
									osMessageQueueReset(Queue1Handle);

									// 2. Reset các chỉ số logic về trạng thái ban đầu
									currentRound = 1;
									shouldEndGame = false;
									shouldStopTask = false;
									shouldStopScreen = false;

									gameInstance.reset();
									gameInstance.score = 0;
									gameInstance.ship.lives = 3;
									hearts = 3;

									// SỬA LỖI KẸT NỀN: Ép hình nền quay về phông nền của Round 1 ngay lập tức
									backgroundImage.setBitmap(touchgfx::Bitmap(BITMAP_BG_ROUND1_ID));
									backgroundImage.invalidate();

									// 3. Giải phóng UI xóa sạch ảnh cũ kẹt trên màn hình
									for (int i = 0; i < MAX_ENEMY; i++) {
										remove(enemyImage[i]);
										enemyImage[i].invalidate();
									}
									for (int i = 0; i < MAX_BULLET; i++) {
										remove(shipBulletImage[i]);
										remove(enemyBulletImage[i]);
										shipBulletImage[i].invalidate();
										enemyBulletImage[i].invalidate();
									}

									// 4. Ẩn và giải phóng bảng thông báo Game Over
									image2.setVisible(false);
									menu_button.setVisible(false);
									score_holder.setVisible(false);
									highscore_holder.setVisible(false);

									remove(image2);
									remove(menu_button);
									remove(score_holder);
									remove(highscore_holder);

									// Làm sáng rực lại 3 trái tim mạng
									heart_01.setAlpha(255);
									heart_02.setAlpha(255);
									heart_03.setAlpha(255);

									// Làm mới lại toàn bộ trạng thái vật lý backend
									resetGameObjectsForNextRound();
									bringUIElementsToFront();
									invalidate();

									// 5. Tạo ngay luồng game mới
									const osThreadAttr_t gameTask_attributes = {
										.name = "gameTask",
										.stack_size = 8192 * 2,
										.priority = (osPriority_t) osPriorityNormal
									};
									gameTaskHandle = osThreadNew(gameTask, NULL, &gameTask_attributes);
								}
							}
	}
