#include <stdlib.h>

#include "device_driver.h"
#include "graphics.h"

#define LCDW (320)
#define LCDH (240)
#define X_MIN (0)
#define X_MAX (LCDW - 1)
#define Y_MIN (0)
#define Y_MAX (LCDH - 1)

#define TIMER_PERIOD (1)
#define RIGHT (1)
#define LEFT (-1)
#define HOME (0)
#define SCHOOL (1)

#define MOVABLE_STEP (10)
#define MOVABLE_SIZE_W (10)
#define MOVABLE_SIZE_H (10)
#define PLAYER_STEP (20)
#define PLAYER_SIZE_W (20)
#define PLAYER_SIZE_H (20)
#define ITEM_SIZE_W (10)
#define ITEM_SIZE_H (10)

#define BACK_COLOR (5)
#define MOVABLE_COLOR (0)
#define PLAYER_COLOR (1)

#define GAME_OVER (1)
#define START (0)
#define OVER (1)
#define CLEAR (2)

unsigned short colour[] = {RED,   YELLOW, GREEN, BLUE,
                           WHITE, BLACK,  GREY,  ORANGE};
enum { STARTSCREEN, SELECTSCREEN, GAME1, GAME2 };

class Game_Object {
 protected:
  int x;
  int y;
  int w;
  int h;
  int color;
  int step;

 public:
  Game_Object(int x = 0, int y = 0, int w = 20, int h = 20, int color = YELLOW,
              int step = 20)
      : x(x), y(y), w(w), h(h), color(color), step(step) {}

  ~Game_Object() {}

  void Set_Object_Pos(int x, int y) {
    this->x = x;
    this->y = y;
  }

  void Clear_Object() { Lcd_Draw_Box(x, y, w, h, BLACK); }
  void Draw_Object() { Lcd_Draw_Box(x, y, w, h, color); }

  int Get_X() const { return x; }
  int Get_Y() const { return y; }
};

class Item : public Game_Object {
 protected:
  int Item_Collision(int player_x, int player_y) {
    if ((x < player_x + PLAYER_SIZE_W) && (x + ITEM_SIZE_W - 1 > player_x) &&
        (y < player_y + PLAYER_SIZE_H) && (y + ITEM_SIZE_H - 1 > player_y)) {
      return 1;
    }
    return 0;
  }

 public:
  Item(int item_x = rand() % 280 + 2, int item_y = rand() % 200 + 20,
       int item_w = 10, int item_h = 10, int color = ORANGE)
      : Game_Object(item_x, item_y, item_w, item_h, color) {}

  void Set_Item_Pos(int x, int y) {
    this->x = x;
    this->y = y;
  }

  void Clear_Item() { Lcd_Draw_Box(x, y, w, h, BLACK); }
  void Draw_Item() { Lcd_Draw_Box(x, y, w, h, color); }
};

class ExScore : public Item {
 private:
  int ExtraScore;

 public:
  ExScore(int item_x = rand() % 280 + 2, int item_y = rand() % 200 + 20,
          int item_w = 10, int item_h = 10, int color = ORANGE,
          int ExtraScore = 10)
      : Item(item_x, item_y, item_w, item_h, color), ExtraScore(ExtraScore) {}

  int Get_Score() const { return ExtraScore; }
  int Get_Collision(int player_x, int player_y) {
    return Item_Collision(player_x, player_y);
  }
};

class Player : public Game_Object {
 private:
  int destination;

  int Position_Change_Player(int key) {
    switch (key) {
      case 0:
        if (y > Y_MIN) y -= PLAYER_STEP;
        break;
      case 1:
        if (y + h < Y_MAX) y += PLAYER_STEP;
        break;
      case 2:
        if (x > X_MIN) x -= PLAYER_STEP;
        break;
      case 3:
        if (x + w < X_MAX) x += PLAYER_STEP;
        break;
      default:
        break;
    }

    return Player_Collision();
  }

  int Player_Collision() {
    if ((destination == SCHOOL) && (y == Y_MIN)) {
      Uart_Printf("SCHOOL\n");
      destination = HOME;
    } else if ((destination == HOME) && (y == LCDH - h)) {
      Uart_Printf("HOME\n");
      destination = SCHOOL;
      return 1;
    }
    return 0;
  }

 public:
  Player(int player_x = 150, int player_y = 100, int player_w = 20,
         int player_h = 20, int color = YELLOW, int player_step = PLAYER_STEP,
         int destination = SCHOOL)
      : Game_Object(player_x, player_y, player_w, player_h, color, player_step),
        destination(destination) {}

  int Move_Player(int key) {
    int collision = 0;
    Clear_Object();
    collision = Position_Change_Player(key);
    Draw_Object();
    return collision;
  }
};

class Movable : public Game_Object {
 protected:
  int direction;
  int speed;

  int Movable_Collision(int player_x, int player_y) {
    if ((x < player_x + PLAYER_SIZE_W) && (x + MOVABLE_SIZE_W - 1 > player_x) &&
        (y < player_y + PLAYER_SIZE_H) && (y + MOVABLE_SIZE_H - 1 > player_y)) {
      return 1;
    }
    return 0;
  }

 public:
  Movable(int movable_x = 0, int movable_y = 100, int color = RED,
          int movable_w = 10, int movable_h = 10, int speed = rand() % 7 + 3,
          int movable_step = MOVABLE_STEP, int direction = RIGHT)
      : Game_Object(movable_x, movable_y, movable_w, movable_h, color,
                    movable_step),
        direction(direction),
        speed(speed) {}

  virtual int Movable_Move(int player_x, int player_y) {
    Clear_Object();
    x += MOVABLE_STEP * direction;
    if (x + w > X_MAX) {
      Set_Object_Pos(X_MIN, Get_Y());

    } else if (x < X_MIN) {
      x = X_MAX - w;
    }
    Draw_Object();

    return Movable_Collision(player_x, player_y);
  }

  int Get_speed() const { return speed; }
};

class BulletLR : public Movable {
 public:
  BulletLR(int movable_x = 0, int movable_y = 100, int color = RED,
           int movable_w = 10, int movable_h = 10, int speed = rand() % 7 + 3,
           int movable_step = MOVABLE_STEP, int direction = RIGHT)
      : Movable(movable_x, movable_y, color, movable_w, movable_h, movable_step,
                speed, direction) {
    Set_Object_Pos(0, Get_BulletLR_Random_YPos());
  }

  virtual int Movable_Move(int player_x, int player_y) override {
    Clear_Object();
    x += MOVABLE_STEP * direction;
    if (x + w > X_MAX) {
      Set_Object_Pos(X_MIN, Get_BulletLR_Random_YPos());

    } else if (x < X_MIN) {
      x = X_MAX - w;
    }
    Draw_Object();

    return Movable_Collision(player_x, player_y);
  }

  int Get_BulletLR_Random_YPos() { return (rand() % 24) * 10; }
};

class BulletUD : public Movable {
 public:
  BulletUD(int movable_x = 0, int movable_y = 100, int color = BLUE,
           int movable_w = 10, int movable_h = 10, int speed = rand() % 7 + 3,
           int movable_step = MOVABLE_STEP, int direction = RIGHT)
      : Movable(movable_x, movable_y, color, movable_w, movable_h, speed,
                movable_step, direction) {
    Set_Object_Pos(Get_BulletUD_Random_XPos(), 0);
  }

  virtual int Movable_Move(int player_x, int player_y) override {
    Clear_Object();
    y += MOVABLE_STEP * direction;
    if (y + h > Y_MAX) {
      Set_Object_Pos(Get_BulletUD_Random_XPos(), Y_MIN);

    } else if (y < Y_MIN) {
      y = Y_MAX - h;
    }
    Draw_Object();

    return Movable_Collision(player_x, player_y);
  }

  int Get_BulletUD_Random_XPos() { return (rand() % 32) * 10; }
};

class Game {
 public:
  static int play;
  static int score;
  static int difficulty;
  static unsigned int time;
  static int Total_Score;
  static int extra_score;

  Game(int play = START) {
    if (play == START) {
      Uart_Printf("Game Start\n");
    } else if (play == OVER) {
      Game_Over();
    }
  }
  void Prt_Score() const { Uart_Printf("Total Score : %d\n", score); }

  void Draw_Start() {
    Lcd_Draw_Box(60, 50, 30, 10, WHITE);
    Lcd_Draw_Box(60, 50, 10, 45, WHITE);
    Lcd_Draw_Box(60, 85, 30, 10, WHITE);
    Lcd_Draw_Box(80, 85, 10, 45, WHITE);
    Lcd_Draw_Box(60, 120, 30, 10, WHITE);

    Lcd_Draw_Box(100, 50, 30, 10, WHITE);
    Lcd_Draw_Box(100, 50, 10, 80, WHITE);
    Lcd_Draw_Box(100, 120, 30, 10, WHITE);

    Lcd_Draw_Box(140, 50, 30, 10, WHITE);
    Lcd_Draw_Box(140, 50, 10, 80, WHITE);
    Lcd_Draw_Box(140, 120, 30, 10, WHITE);
    Lcd_Draw_Box(160, 50, 10, 80, WHITE);

    Lcd_Draw_Box(180, 50, 10, 80, RED);
    Lcd_Draw_Box(180, 50, 25, 10, RED);
    Lcd_Draw_Box(180, 75, 30, 10, RED);
    Lcd_Draw_Box(195, 50, 10, 25, RED);
    Lcd_Draw_Box(200, 85, 10, 55, RED);

    Lcd_Draw_Box(220, 50, 10, 80, WHITE);
    Lcd_Draw_Box(220, 50, 30, 10, WHITE);
    Lcd_Draw_Box(220, 85, 30, 10, WHITE);
    Lcd_Draw_Box(220, 120, 30, 10, WHITE);

    Lcd_Printf(110, 180, WHITE, BLACK, 1, 1, "Press START");
  }

  void Game_Over() {
    Uart_Printf("Game Over\n");
    Prt_Score();
  }

  void Game_Clear() {
    Uart_Printf("Game Clear\n");
    Prt_Score();
  }

  void Score() {}

  int Get_Play() { return play; }

  int Check_EndCondition() {
    if (play == OVER) {
      Game_Over();
      return 1;
    } else if (play == CLEAR) {
      Game_Clear();
      return 1;
    }
    return 0;
  }

  void ReStart() {
    play = START;
    score = 0;
    difficulty = 0;
    time = 0;
    Total_Score = 0;
    extra_score = 0;
  }

  void Spawn_Movables() {}

  void Move_Movables() {}

  void Movables_Pos_Reset() {}

  static void Time_Tick() { time++; };
};

class Mode1 : public Game {
 public:
  void Score() {
    score++;
    if (score == 5) {
      play = CLEAR;
    }
    difficulty = score;
    Lcd_Printf(240, 0, WHITE, BLACK, 1, 1, "Score : %d", score);
  }

  void Spawn_Movables(Movable *movable) {
    if (score > 5) {
      score = 5;
    }
    for (int i = 0; i < difficulty + 1; i++) {
      movable[i].Draw_Object();
    }
  }

  void Move_Movables(Movable *movable, Player &player) {
    for (int i = 0; i < difficulty + 1; i++) {
      if (time % movable[i].Get_speed() == 0) {
        if (movable[i].Movable_Move(player.Get_X(), player.Get_Y())) {
          play = OVER;
        }
      }
    }
  }

  void Movables_Pos_Reset(Movable *movable) {
    for (int i = 0; i < 5; i++) {
      movable[i].Set_Object_Pos(0, movable[i].Get_Y());
    }
  }
};

class Mode2 : public Game {
 public:
  void Score() {
    if (time % 100 == 0) {
      score++;
      if (score % 5 == 0) {
        difficulty++;
      }
    }
    Total_Score = score + extra_score;
    Lcd_Printf(240, 0, WHITE, BLACK, 1, 1, "Score : %d", Total_Score);
  }

  void Spawn_Movables(BulletLR *bullet, BulletUD *bullet2) {
    if (difficulty > 9) {
      difficulty = 9;
    }
    for (int i = 0; i < difficulty + 1; i++) {
      bullet[i].Draw_Object();
      bullet2[i].Draw_Object();
    }
  }

  void Move_Movables(BulletLR *bullet, BulletUD *bullet2, Player &player) {
    for (int i = 0; i < difficulty + 1; i++) {
      if (time % bullet[i].Get_speed() == 0) {
        if (bullet[i].Movable_Move(player.Get_X(), player.Get_Y())) {
          play = OVER;
        }
      }
      if (time % bullet2[i].Get_speed() == 0) {
        if (bullet2[i].Movable_Move(player.Get_X(), player.Get_Y())) {
          play = OVER;
        }
      }
    }
  }

  void Movables_Pos_Reset(BulletLR *bullet, BulletUD *bullet2) {
    for (int i = 0; i < 10; i++) {
      bullet[i].Set_Object_Pos(0, bullet[i].Get_BulletLR_Random_YPos());
      bullet2[i].Set_Object_Pos(bullet2[i].Get_BulletUD_Random_XPos(), 0);
    }
  }

  void Spawn_Items(ExScore *item, Player &player) {
    static int item_spawn_time = -1;  // 아이템 생성 시간을 기록
    static bool item_active = false;  // 아이템 활성화 상태

    if (time % 1000 == 0 && !item_active) {
      item->Set_Item_Pos(rand() % (X_MAX - ITEM_SIZE_W),
                         rand() % (Y_MAX - ITEM_SIZE_H));
      item->Draw_Item();
      item_spawn_time = time;
      item_active = true;
    }

    if (item_active) {
      if (item->Get_Collision(player.Get_X(), player.Get_Y())) {
        extra_score = extra_score + item->Get_Score();  // ExScore를 올림
        item_active = false;
        item->Clear_Item();
      } else if (time - item_spawn_time >= 3000) {  // 3초가 지나면 아이템 제거
        item_active = false;
        item->Clear_Item();
      }
    }
  }
};

int Game::score = 0;
int Game::play = START;
unsigned int Game::time = 0;
int Game::difficulty = 0;
int Game::Total_Score = 0;
int Game::extra_score = 0;

void Game_Init1(Player &player, Movable *movable, Mode1 &game) {
  Lcd_Clr_Screen();
  game.ReStart();

  player.Set_Object_Pos(150, 220);
  player.Draw_Object();

  game.Movables_Pos_Reset(movable);
  game.Spawn_Movables(movable);
}

void Game_Init2(Player &player, BulletLR *bullet, BulletUD *bullet2,
                Mode2 &game) {
  Lcd_Clr_Screen();
  game.ReStart();

  player.Set_Object_Pos(150, 100);
  player.Draw_Object();

  game.Movables_Pos_Reset(bullet, bullet2);
  game.Spawn_Movables(bullet, bullet2);
}

extern volatile int TIM4_expired;
extern volatile int USART1_rx_ready;
extern volatile int USART1_rx_data;
extern volatile int Jog_key_in;
extern volatile int Jog_key;

extern "C" void abort(void) { while (1); }

static void Sys_Init(void) {
  Clock_Init();
  LED_Init();
  Uart_Init(115200);

  SCB->VTOR = 0x08003000;
  SCB->SHCSR = 7 << 16;
}

extern "C" void Main() {
  Sys_Init();
  Uart_Printf("Game Example\n");

  Lcd_Init();
  Jog_Poll_Init();
  Jog_ISR_Enable(1);
  Uart1_RX_Interrupt_Enable(1);
  Game game(START);
  Player player;
  Movable movable[5] = {
      Movable(0, 120, RED, 20, 20), Movable(0, 100, GREY, 40, 20),
      Movable(0, 80, GREEN, 20, 10), Movable(0, 60, BLUE, 20, 20),
      Movable(0, 40, WHITE, 40, 20)};
  BulletLR bullet[10];
  BulletUD bullet2[10];
  ExScore item;

  int GameMode = 1;
  int GameState = STARTSCREEN;
  Jog_key = -1;

  for (;;) {
    if (GameState == STARTSCREEN) {
      while (Jog_key != 5) {
        game.Draw_Start();
        GameState = SELECTSCREEN;
        TIM2_Delay(50);
        Jog_key_in = 0;
      }
      Lcd_Clr_Screen();
      Jog_key = -1;
    }

    else if (GameState == SELECTSCREEN) {
      while (Jog_key != 5) {
        switch (GameMode) {
          case 1:
            Lcd_Printf(35, 65, WHITE, BLACK, 2, 1, "1. CROSS THE ROAD");
            Lcd_Printf(35, 105, GREY, BLACK, 2, 1, "2. EVADE BULLET");
            Lcd_Printf(85, 165, RED, BLACK, 1, 1, "Press SW1 to select");
            Lcd_Printf(200, 200, BLUE, BLACK, 1, 1, "Made By Team 4");
            GameState = GAME1;
            if (Jog_key == 1) {
              GameMode = 2;
              Jog_key = -1;
            }
            break;
          case 2:
            Lcd_Printf(35, 65, GREY, BLACK, 2, 1, "1. CROSS THE ROAD");
            Lcd_Printf(35, 105, WHITE, BLACK, 2, 1, "2. EVADE BULLET");
            Lcd_Printf(85, 165, RED, BLACK, 1, 1, "Press SW1 to select");
            Lcd_Printf(200, 200, BLUE, BLACK, 1, 1, "Made By Team 4");
            GameState = GAME2;
            if (Jog_key == 0) {
              GameMode = 1;
              Jog_key = -1;
            }
            break;
        }
        TIM2_Delay(50);
        Jog_key_in = 0;
      }
    }

    else if (GameState == GAME1) {
      Mode1 game1;
      for (;;) {
        Game_Init1(player, movable, game1);
        TIM4_Repeat_Interrupt_Enable(1, TIMER_PERIOD * 10);

        for (;;) {
          if (Jog_key_in) {
            if (player.Move_Player(Jog_key)) {
              game1.Score();
              game1.Prt_Score();
            }
            Jog_key_in = 0;
          }
          if (TIM4_expired) {
            Game::Time_Tick();
            game1.Move_Movables(movable, player);
            TIM4_expired = 0;
          }
          if (game1.Check_EndCondition()) {
            TIM4_Repeat_Interrupt_Enable(0, 0);
            Lcd_Clr_Screen();
            Lcd_Printf(180, 0, WHITE, BLACK, 1, 1, "Total Score : %d",
                       game1.score);
            Uart_Printf("Press SW0 key to restart the game...\n");
            Uart_Printf("Press SW1 key to select game mode...\n");
            Lcd_Printf(20, 80, WHITE, BLACK, 1, 2,
                       "Press SW0 key to restart the game...");
            Lcd_Printf(20, 110, WHITE, BLACK, 1, 2,
                       "Press SW1 key to select game mode...");

            for (;;) {
              if (Jog_key_in) {
                if (Jog_key == 4) {
                  TIM2_Delay(50);
                  Jog_key_in = 0;
                  break;
                }

                if (Jog_key == 5) {
                  GameState = SELECTSCREEN;
                  TIM2_Delay(50);
                  Jog_key_in = 0;
                  break;
                }
              }
            }
            break;
          }
        }
        Lcd_Clr_Screen();
        Jog_key = -1;
        break;
      }
    }

    else if (GameState == GAME2) {
      Mode2 game2;
      for (;;) {
        Game_Init2(player, bullet, bullet2, game2);
        TIM4_Repeat_Interrupt_Enable(1, TIMER_PERIOD * 10);

        for (;;) {
          if (Jog_key_in) {
            if (player.Move_Player(Jog_key)) {
            }
            Jog_key_in = 0;
          }

          if (TIM4_expired) {
            Game::Time_Tick();
            game2.Score();
            game2.Move_Movables(bullet, bullet2, player);
            game2.Spawn_Items(&item, player);
            TIM4_expired = 0;
          }

          if (game2.Check_EndCondition()) {
            TIM4_Repeat_Interrupt_Enable(0, 0);
            Lcd_Clr_Screen();
            Lcd_Printf(180, 0, WHITE, BLACK, 1, 1, "Total Score : %d",
                       game2.Total_Score);
            Uart_Printf("Press SW0 key to restart the game...\n");
            Uart_Printf("Press SW1 key to select game mode...\n");
            Lcd_Printf(20, 80, WHITE, BLACK, 1, 2,
                       "Press SW0 key to restart the game...");
            Lcd_Printf(20, 110, WHITE, BLACK, 1, 2,
                       "Press SW1 key to select game mode...");

            for (;;) {
              if (Jog_key_in) {
                if (Jog_key == 4) {
                  TIM2_Delay(50);
                  Jog_key_in = 0;
                  break;
                }

                if (Jog_key == 5) {
                  GameState = SELECTSCREEN;
                  TIM2_Delay(50);
                  Jog_key_in = 0;
                  break;
                }
              }
            }
            break;
          }
        }
        Lcd_Clr_Screen();
        Jog_key = -1;
        break;
      }
    }
  }
}