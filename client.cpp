#include "bullet.hpp"
#include "drawScale.hpp"
#include "game.hpp"
#include "math.h"
#include "player.hpp"
#include "raylib.h"
#include "raymath.h"
#include "utils.hpp"
#include "constants.hpp"
#include <arpa/inet.h>
#include <atomic>
#include <cstdio>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

int sock = socket(AF_INET, SOCK_STREAM, 0);

std::mutex packets_mutex;
std::list<std::string> packets = {};

std::atomic<bool> running = true;

void do_recv() {
  char buffer[1024];

  while (running) {
    int bytes = recv(sock, &buffer, sizeof(buffer), 0);

    if (bytes == 0) {
      std::cout << "Server disconnected.\n";
      running = false;
      break;
    } else if (bytes < 0) {
      perror("Error receiving packet");
      running = false;
      break;
    }

    std::lock_guard<std::mutex> lock(packets_mutex);
    std::string packet(buffer, bytes);

    packets.push_back(packet);
  }
}

void handle_packet(int packet_type, std::string payload, Game *game,
                   int *my_id) {
  std::istringstream in(payload);
  std::vector<std::string> msg_split;

  if (packet_type != 2) {
    std::cout << payload << std::endl;
  }

  switch (packet_type) {
  case 0:
    // remove trailing semicolon if present
    if (!payload.empty() && payload.back() == ';') {
      payload.pop_back();
    }
    split(payload, std::string(":"), msg_split);
    for (std::string i : msg_split) {
      if (i == std::string(""))
        continue;

      // split each player entry by spaces
      std::vector<std::string> player_parts;
      split(i, std::string(" "), player_parts);

      if (player_parts.size() < 5) {
        std::cerr << "Invalid player data: " << i << std::endl;
        continue;
      }

      try {
        int id = std::stoi(player_parts[0]);
        int x = std::stoi(player_parts[1]);
        int y = std::stoi(player_parts[2]);
        // join all fields between index 3 and last-1 as the username
        std::string username;
        for (size_t j = 3; j + 1 < player_parts.size(); ++j) {
          if (!username.empty())
            username += " ";
          username += player_parts[j];
        }
        // the color should be the last element
        unsigned int color_code =
            (unsigned int)std::stoi(player_parts[player_parts.size() - 1]);

        (*game).players[id] = Player(x, y);
        (*game).players[id].username = username;
        (*game).players[id].color = uint_to_color(color_code);

        std::cout << "Player " << id << " color code: " << color_code
                  << std::endl;
      } catch (const std::exception &e) {
        std::cerr << "Error parsing player data: " << i << " - " << e.what()
                  << std::endl;
      }
    }
    break;

  case 1:
    in >> *my_id;
    break;
  case 2: {
    std::istringstream i(payload);
    int id, x, y;
    float rot;
    i >> id >> x >> y >> rot;

    if ((*game).players.find(id) != (*game).players.end()) {
      (*game).players.at(id).nx = x;
      (*game).players.at(id).ny = y;
      (*game).players.at(id).rot = rot;
    }

  } break;
  case 3:
    split(payload, " ", msg_split);
    {
      int id = std::stoi(msg_split.at(0));
      int x = std::stoi(msg_split.at(1));
      int y = std::stoi(msg_split.at(2));
      std::string username = msg_split.at(3);

      (*game).players[id] = Player(x, y);
      (*game).players[id].username = username;
      (*game).players[id].color = uint_to_color(std::stoi(msg_split.at(4)));
    }

    break;
  case 4:
    if ((*game).players.find(std::stoi(payload)) != (*game).players.end())
      (*game).players.erase(std::stoi(payload));
    break;
  case 5: {
    split(payload, std::string(" "), msg_split);
    (*game).players.at(std::stoi(msg_split[0])).username = msg_split[1];
  } break;
  case 6: {
    split(payload, std::string(" "), msg_split);
    if ((*game).players.find(std::stoi(msg_split[0])) !=
        (*game).players.end()) {
      (*game).players.at(std::stoi(msg_split[0])).color =
          uint_to_color(std::stoi(msg_split[1]));
    }
  } break;
  }
}

void handle_packets(Game *game, int *my_id) {
  std::lock_guard<std::mutex> lock(packets_mutex);
  while (!packets.empty()) {
    std::string packet = packets.front();

    int packet_type = std::stoi(packet.substr(0, packet.find('\n')));
    std::string payload =
        packet.substr(packet.find('\n') + 1, packet.find_first_of(';') - 2);

    handle_packet(packet_type, payload, game, my_id);

    packets.pop_front();
  }
}

void do_username_prompt(std::string *usernameprompt, bool *usernamechosen,
                        playermap *players, int my_id, int *mycolor,
                        Color options[5]) {

  BeginDrawing();
  ClearBackground(BLACK);
  
  BeginUiDrawing();
  DrawTextScaleCentered("Pick a username", 50, 50, 64, WHITE);

  DrawTextScaleCentered((std::to_string(10 - (*usernameprompt).length())
                 .append(" characters left."))
                    .c_str(),
                50, 215,
           32, (*usernameprompt).length() < 10 ? WHITE : RED);
  DrawRectangleScaleCentered(50, 125, 500, 75, BLUE);

  DrawTextScaleCentered((*usernameprompt).c_str(), 75, 150, 48, BLACK);

  // Color Pick
  DrawTextScaleCentered("Choose color:", 50, 300, 32, WHITE);

  int x = 50;

  for (int i = 0; i < 5; i++) {
    Color cc = options[i];
    if (color_equal(cc, options[*mycolor])) {
      DrawSquareScaleCentered(x - 10, 340, 70, WHITE);
    }
    DrawSquareScaleCentered(x, 350, 50, cc);

    x += 100;
  }
  EndUiDrawing();

  char k = GetCharPressed();

  if (k != 0 && k != '\n' && k != ':' && k != ' ' && k != ';' &&
      (*usernameprompt).length() < 10) {
    (*usernameprompt).push_back(k);
  }
  if (IsKeyPressed(KEY_BACKSPACE) && !(*usernameprompt).empty())
    (*usernameprompt).pop_back();

  if (IsKeyPressed(KEY_ENTER) && !(*usernameprompt).empty() && my_id != -1) {
    (*players)[my_id].username = *usernameprompt;
    *usernamechosen = true;
    send_message(std::string("5\n").append(*usernameprompt), sock);
    sleep(2);
    send_message("6\n" + std::to_string(color_to_uint(options[*mycolor])),
                 sock);
  }

  if (IsKeyPressed(KEY_LEFT))
    (*mycolor)--;
  if (IsKeyPressed(KEY_RIGHT))
    (*mycolor)++;
  if (*mycolor < 0)
    *mycolor = 4;
  if (*mycolor > 4)
    *mycolor = 0;

  EndDrawing();
}

void draw_ui(Color mycolor, playermap players, int my_id, int bd) {
  BeginUiDrawing();
  
  // Calculate text width
  float textWidth = MeasureText(players[my_id].username.c_str(), 24);
  
  // Base dimensions (before scaling)
  float boxHeight = 50;
  float boxWidth = boxHeight * 2;
  float padding = 5;
  
  // Position at bottom of window_size
  float y = window_size.y - boxHeight;
  
  // Draw background
  DrawRectangleScale(0, y, boxWidth, boxHeight, DARKGRAY);
  
  // Player color square (40x40)
  DrawSquareScale(padding, y + (boxHeight - 40)/2, 40, mycolor);
  
  // Username text aligned with the square
  DrawTextScale(players[my_id].username.c_str(), 50, y + padding, 24, WHITE);
  
  // Cooldown bar aligned with username
  if (bd != 0)
    DrawRectangleScale(50, y + boxHeight - padding - 10, bd * 2, 10, GREEN);
  
  EndUiDrawing();
}

void draw_players(playermap players,
                  std::map<Color, Texture2D, ColorCompare> player_textures) {
  for (auto &[id, p] : players) {
    if (p.username == "unset")
      continue;
    Color clr = p.color;
    if (player_textures.find(clr) != player_textures.end())
      DrawTexture(player_textures[clr], p.x, p.y, WHITE);

    DrawText(p.username.c_str(),
             p.x + 50 - MeasureText(p.username.c_str(), 32) / 2, p.y - 50, 32,
             BLACK);

    DrawRectanglePro({(float)p.x + 50, (float)p.y + 50, 50, 20},
                     {(float)100, (float)0}, p.rot, BLACK);
  }
}

bool move_gun(float *rot, int cx, int cy, Camera2D cam) {
  Vector2 mousePos = GetScreenToWorld2D(GetMousePosition(), cam);
  Vector2 playerCenter = {(float)cx + 50, (float)cy + 50};
  Vector2 delta = Vector2Subtract(playerCenter, mousePos);
  float angle = atan2f(delta.y, delta.x) * RAD2DEG;
  float oldrot = *rot;
  *rot = fmod(angle + 360.0f, 360.0f);

  return *rot == oldrot;
}

void move_players(playermap *players, int skip) {
  for (auto &[k, v] : *players) {
    if (k == skip)
      continue;
    if (v.x != v.nx) {
      if (v.x < v.nx)
        v.x += v.speed;
      if (v.x > v.nx)
        v.x -= v.x;
    }
    if (v.y != v.ny) {
      if (v.y < v.ny)
        v.y += v.speed;
      if (v.y > v.ny)
        v.y -= v.y;
    }
  }
}

int main() {
  sockaddr_in sock_addr;
  sock_addr.sin_family = AF_INET;
  sock_addr.sin_port = htons(50000);
  sock_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

  if (connect(sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0) {
    perror("Could not connect to server");

    close(sock);
    return -1;
  }

  std::thread recv_thread(do_recv);

  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(window_size.x, window_size.y, "Multi Ludens");

  SetTargetFPS(60);

  Image floorImage = LoadImage("floor_tile.png");
  ImageResizeNN(&floorImage, 100, 100);
  Texture2D floorTexture = LoadTextureFromImage(floorImage);
  Game game;
  std::vector<Bullet> bullets;

  std::map<Color, Texture2D, ColorCompare> player_textures;
  player_textures[RED] = LoadTexture("player_red.png");
  player_textures[GREEN] = LoadTexture("player_green.png");
  player_textures[YELLOW] = LoadTexture("player_yellow.png");
  player_textures[PURPLE] = LoadTexture("player_purple.png");
  player_textures[ORANGE] = LoadTexture("player_orangle.png");

  for (auto &[_, v] : player_textures) {
    Image a = LoadImageFromTexture(v);
    ImageResizeNN(&a, 100, 100);
    v = LoadTextureFromImage(a);
  }

  int my_id = -1;
  int server_update_counter = 0;
  bool hasmoved = false;
  bool usernamechosen = false;
  std::string usernameprompt;
  Color mycolor = BLACK;
  int colorindex = 0;
  int bdelay = 20;
  int canshoot = false;

  Color options[5] = {RED, GREEN, YELLOW, PURPLE, ORANGE};

  Camera2D cam;
  cam.zoom = 1.0f;
  cam.rotation = 0.0f;
  cam.offset = {0.0f, 0.0f};

  while (!WindowShouldClose() && running) {
    cam.zoom = (GetScreenWidth() / window_size.x + GetScreenHeight() / window_size.y) / 2;
    // cam.offset = {(float)GetScreenWidth() / 3.0f, (float)GetScreenHeight() / 3.0f};

    int cx = game.players[my_id].x;
    int cy = game.players[my_id].y;

    cam.target = {(float)cx - 350, (float)cy - 250};

    handle_packets(&game, &my_id);

    if (my_id == -1) {
      BeginDrawing();
      ClearBackground(BLACK);
      DrawTextScale("waiting for server...", 50, 50, 48, GREEN);
      DrawTextScale("loading...", 50, 100, 32, GREEN);
      EndDrawing();
      continue;
    }

    if (!usernamechosen) {
      do_username_prompt(&usernameprompt, &usernamechosen, &game.players, my_id,
                         &colorindex, options);
      continue;
    }

    if (mycolor.r == 0 && mycolor.g == 0 && mycolor.b == 0) {
      std::cout << colorindex << std::endl;
      mycolor = options[colorindex];
      game.players[my_id].color = mycolor;
    }

    server_update_counter++;

    bool moved = game.players.at(my_id).move();
    bool moved_gun = move_gun(&game.players[my_id].rot, cx, cy, cam);

    hasmoved = moved || moved_gun;

    if (server_update_counter >= 5 && hasmoved) {
      std::string msg =
          std::string("2\n" + std::to_string(game.players.at(my_id).x) + " " +
                      std::to_string(game.players.at(my_id).y) + " " +
                      std::to_string(game.players.at(my_id).rot));
      send_message(msg, sock);

      server_update_counter = 0;
    }

    move_players(&game.players, my_id);

    for (Bullet &b : bullets)
      b.move();

    if (!canshoot)
      bdelay--;
    if (!canshoot && bdelay == 0) {
      canshoot = true;
    }

    if (IsMouseButtonDown(0) && canshoot) {
      canshoot = false;
      bdelay = 20;
      float bspeed = 10;
      float angleRad = (-game.players[my_id].rot + 5) * DEG2RAD;
      Vector2 dir = Vector2Scale({cosf(angleRad), -sinf(angleRad)}, -bspeed);
      Vector2 spawnOffset =
          Vector2Scale({cosf(angleRad), -sinf(angleRad)}, -120);
      Vector2 origin = {(float)game.players[my_id].x + 50,
                        (float)game.players[my_id].y + 50};
      Vector2 spawnPos = Vector2Add(origin, spawnOffset);
      bullets.push_back(Bullet(spawnPos.x, spawnPos.y, dir));

      send_message(
          std::string("10\n ").append(std::to_string(game.players[my_id].rot)),
          sock);
    }

    // ------------------------------------------------------------------------------

    float dt = GetFrameTime();

    cam.target = {(float)game.players[my_id].x - 350,
                  (float)game.players[my_id].y - 250};

    BeginDrawing();

    ClearBackground(BLUE);

    DrawFPS(2, 2);

    BeginMode2D(cam);

    // Draw floor tiles based on PLAYING_AREA
    for (int i = 0; i < PLAYING_AREA.width/TILE_SIZE; i++) {
      for (int j = 0; j < PLAYING_AREA.height/TILE_SIZE; j++) {
        DrawTexture(floorTexture, i * TILE_SIZE, j * TILE_SIZE, WHITE);
      }
    }

    draw_players(game.players, player_textures);

    for (Bullet &b : bullets)
      b.show();

    EndMode2D();

    draw_ui(mycolor, game.players, my_id, bdelay);

    EndDrawing();
  }

  std::cout << "Closing.\n";

  running = false;

  shutdown(sock, SHUT_RDWR);
  close(sock);

  recv_thread.join();

  CloseWindow();
}
