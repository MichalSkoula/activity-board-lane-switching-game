#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 64    // OLED display height, in pixels
#define OLED_RESET    -1    // Reset pin (-1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // Typical I2C address for 0.96" OLED displays

// Joystick pins
#define JOYSTICK_X_PIN A1  // X axis
#define JOYSTICK_Y_PIN A0  // Y axis
#define JOYSTICK_BTN_PIN 2 // Click/Button (D1)
#define BUZZER_PIN 4       // Buzzer/Speaker pin

// Game constants
#define LANE_WIDTH 32      // Width of each lane
#define LANE_1_CENTER 21   // Center of the left lane (x-coordinate)
#define LANE_2_CENTER 64   // Center of the middle lane (x-coordinate)
#define LANE_3_CENTER 107  // Center of the right lane (x-coordinate)
#define CAR_WIDTH 16       // Width of the car
#define CAR_HEIGHT 16      // Height of the car
#define PLAYER_Y 48        // Y position of player car
#define MAX_OBSTACLES 3    // Back to 3 obstacles with 3 lanes
#define GAME_SPEED 30      // Maintain fast game speed
#define JOYSTICK_THRESHOLD 400 // Increased threshold to make joystick less sensitive (was 250)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Game variables
int playerLane = 1;      // 0 = left lane, 1 = middle lane, 2 = right lane
unsigned long lastMove = 0;
unsigned long lastObstacleSpawn = 0;
unsigned int score = 0;
bool gameOver = false;

// Obstacle structure
struct Obstacle {
  int x;
  int y;
  bool active;
};

Obstacle obstacles[MAX_OBSTACLES];

void setup() {
  Serial.begin(9600);
  
  // Initialize joystick and buzzer pins
  pinMode(JOYSTICK_X_PIN, INPUT);
  pinMode(JOYSTICK_Y_PIN, INPUT);
  pinMode(JOYSTICK_BTN_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Initialize OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  // Show splash screen
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(8, 20);
  display.println(F("KUBA RIDIC"));
  display.setCursor(15, 35);
  display.println(F("Vyhybej se autum"));
  display.display();
  delay(2000);
  
  // Startup sound
  playStartSound();
  
  // Initialize obstacles
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    obstacles[i].active = false;
  }
  
  // Start game
  display.clearDisplay();
}

void loop() {
  if (gameOver) {
    showGameOver();
    
    // Check for button press to restart
    if (digitalRead(JOYSTICK_BTN_PIN) == LOW) {
      resetGame();
    }
    return;
  }
  
  // Read joystick
  int xValue = analogRead(JOYSTICK_X_PIN);
  
  // Handle player movement with less sensitive joystick controls
  if (millis() - lastMove > 300) { // Increased from 50ms to 300ms to reduce sensitivity
    if (xValue > 1023 - JOYSTICK_THRESHOLD && playerLane < 2) {
      // Move right
      playerLane++;
      playLaneChangeSound();
      lastMove = millis();
    } else if (xValue < JOYSTICK_THRESHOLD && playerLane > 0) {
      // Move left
      playerLane--;
      playLaneChangeSound();
      lastMove = millis();
    }
  }
  
  // Spawn obstacles less frequently (4500ms) to give more space between cars
  if (millis() - lastObstacleSpawn > 4500) {
    spawnObstacle();
    lastObstacleSpawn = millis();
  }
  
  // Update obstacles
  updateObstacles();
  
  // Check collisions
  if (checkCollisions()) {
    playCrashSound();
    gameOver = true;
  }
  
  // Draw everything
  display.clearDisplay();
  drawLanes();
  drawPlayer();
  drawObstacles();
  drawScore();
  display.display();
  
  // Game runs faster but difficulty increase is even slower
  int gameDelay = max(30, GAME_SPEED - (score / 30)); // Even slower difficulty increase
  delay(gameDelay);
}

void drawLanes() {
  // Draw lane dividers for 3 lanes
  for (int y = 0; y < SCREEN_HEIGHT; y += 8) {
    display.drawLine(42, y, 42, y + 4, SSD1306_WHITE); // Divider between lanes 1 and 2
    display.drawLine(85, y, 85, y + 4, SSD1306_WHITE); // Divider between lanes 2 and 3
  }
  
  // No horizontal safe zone indicator
}

void drawPlayer() {
  int playerX = 0;
  
  // Set player position based on lane
  switch(playerLane) {
    case 0:
      playerX = LANE_1_CENTER - (CAR_WIDTH / 2);
      break;
    case 1:
      playerX = LANE_2_CENTER - (CAR_WIDTH / 2);
      break;
    case 2:
      playerX = LANE_3_CENTER - (CAR_WIDTH / 2);
      break;
  }
  
  // Draw player car as a filled rectangle
  display.fillRect(playerX, PLAYER_Y, CAR_WIDTH, CAR_HEIGHT, SSD1306_WHITE);
  
  // Add a larger empty inside area (removes the thick borders)
  display.fillRect(playerX + 2, PLAYER_Y + 2, CAR_WIDTH - 4, CAR_HEIGHT - 4, SSD1306_BLACK);
  
  // Add letter "K" to the car
  // Vertical line of K
  display.drawLine(playerX + 5, PLAYER_Y + 5, playerX + 5, PLAYER_Y + 11, SSD1306_WHITE);
  // Top diagonal line of K
  display.drawLine(playerX + 5, PLAYER_Y + 7, playerX + 10, PLAYER_Y + 5, SSD1306_WHITE);
  // Bottom diagonal line of K
  display.drawLine(playerX + 5, PLAYER_Y + 7, playerX + 10, PLAYER_Y + 11, SSD1306_WHITE);
}

void spawnObstacle() {
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (!obstacles[i].active) {
      obstacles[i].active = true;
      obstacles[i].y = -CAR_HEIGHT;
      
      // Randomly select one of three lanes
      int lane = random(3); // 0, 1, or 2
      
      switch(lane) {
        case 0:
          obstacles[i].x = LANE_1_CENTER;
          break;
        case 1:
          obstacles[i].x = LANE_2_CENTER;
          break;
        case 2:
          obstacles[i].x = LANE_3_CENTER;
          break;
      }
      
      break;
    }
  }
}

void updateObstacles() {
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (obstacles[i].active) {
      // Move obstacle down at a moderate pace (faster than 1 but not as fast as 2)
      obstacles[i].y += 2;
      
      // Check if obstacle is off-screen
      if (obstacles[i].y > SCREEN_HEIGHT) {
        obstacles[i].active = false;
        score++;
      }
    }
  }
}

void drawObstacles() {
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (obstacles[i].active) {
      int obstacleX = obstacles[i].x - (CAR_WIDTH / 2);
      
      // Draw obstacle car - slightly smaller than player
      display.fillRect(obstacleX, obstacles[i].y, CAR_WIDTH - 2, CAR_HEIGHT - 2, SSD1306_WHITE);
      
      // Add car details (windows)
      display.fillRect(obstacleX + 2, obstacles[i].y + 2, CAR_WIDTH - 6, 2, SSD1306_BLACK);
      display.fillRect(obstacleX + 2, obstacles[i].y + CAR_HEIGHT - 6, CAR_WIDTH - 6, 2, SSD1306_BLACK);
    }
  }
}

void drawScore() {
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.print(score);
}

bool checkCollisions() {
  int playerX;
  
  // Get player's current lane center
  switch(playerLane) {
    case 0:
      playerX = LANE_1_CENTER;
      break;
    case 1:
      playerX = LANE_2_CENTER;
      break;
    case 2:
      playerX = LANE_3_CENTER;
      break;
  }
  
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (obstacles[i].active) {
      // Check if obstacle is in the same lane and overlapping the player
      if (obstacles[i].x == playerX && 
          obstacles[i].y + CAR_HEIGHT > PLAYER_Y && 
          obstacles[i].y < PLAYER_Y + CAR_HEIGHT) {
        return true;
      }
    }
  }
  
  return false;
}

void showGameOver() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 10);
  display.println(F("KONEC HRY"));
  
  display.setTextSize(1);
  display.setCursor(10, 35);
  display.print(F("Finalni skore: "));
  display.print(score);
  
  display.setCursor(10, 50);
  display.println(F("Zmackni pro restart"));
  
  display.display();
  
  // Shorter delay before allowing restart
  delay(500);
}

// Sound effect functions
void playLaneChangeSound() {
  // Short beep for lane change
  tone(BUZZER_PIN, 2000, 50); // 2kHz tone for 50ms
}

void playCrashSound() {
  // Descending tone for crash
  for (int i = 2000; i > 200; i -= 100) {
    tone(BUZZER_PIN, i, 50);
    delay(30);
  }
  // Final longer low tone
  tone(BUZZER_PIN, 200, 300);
  delay(300);
}

void playStartSound() {
  // Ascending tones for game start
  for (int i = 500; i < 2000; i += 500) {
    tone(BUZZER_PIN, i, 100);
    delay(120);
  }
}

void resetGame() {
  score = 0;
  gameOver = false;
  playerLane = 1; // Start in middle lane
  
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    obstacles[i].active = false;
  }
  
  // Show "Get Ready" message
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(5, 20);
  display.println(F("PRIPRAV SE"));
  display.display();
  
  // Play start sound
  playStartSound();
  
  delay(1500);
  
  lastObstacleSpawn = millis();
}
