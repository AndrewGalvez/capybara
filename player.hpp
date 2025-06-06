#ifndef PLAYER_H
#define PLAYER_H

#include <raylib.h>
#include <string>
struct Player {
public:
  int x = 0;
  int y = 0;
  int nx = 0;
  int ny = 0;
  int speed = 2;
  Color color = RED;
  std::string username = std::string("unset");
  float rot = 0;

  Player(int x, int y) : x(x), y(y), nx(x), ny(y) {}

  Player() : x(100), y(100), nx(100), ny(100) {};

  bool move() {
    bool out = IsKeyDown(KEY_W) || IsKeyDown(KEY_A) || IsKeyDown(KEY_S) ||
               IsKeyDown(KEY_D);
    int dir_x = 0;
    int dir_y = 0;

    if (IsKeyDown(KEY_W))
      dir_y -= speed;
    if (IsKeyDown(KEY_S))
      dir_y += speed;
    if (IsKeyDown(KEY_A))
      dir_x -= speed;
    if (IsKeyDown(KEY_D))
      dir_x += speed;

    this->x += dir_x;
    this->y += dir_y;

    return out;
  }
};

#endif
