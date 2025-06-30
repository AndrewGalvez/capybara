#pragma once
#include "raylib.h"
#include "utils.hpp"
#include <map>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

class ResourceManager {
private:
  std::map<std::string, Texture2D> textures;
  std::map<std::string, Image> images;
  Image player_monochrome;
  std::map<Color, Texture2D> colored_players;
  float color_multiplier_r = 2.0f;
  float color_multiplier_g = 1.0f;
  float color_multiplier_b = 2.0f;

  void load_color_config() {
    std::ifstream config_file("assets/color_config.txt");
    if (config_file.is_open()) {
      config_file >> color_multiplier_r >> color_multiplier_g >> color_multiplier_b;
      config_file.close();
      std::cout << "Loaded color multipliers: R=" << color_multiplier_r 
                << " G=" << color_multiplier_g 
                << " B=" << color_multiplier_b << std::endl;
    } else {
      std::cout << "Could not open color config file, using defaults" << std::endl;
    }
  }

public:
  ResourceManager() {
    player_monochrome = LoadImage("assets/player_monochrome.png");
    load_color_config();
  }

  void reload_color_config() {
    for (auto &[color, tex] : colored_players) {
      UnloadTexture(tex);
    }
    colored_players.clear();
    load_color_config();
  }

  Texture2D getTex(std::string path) {
    if (textures.find(path) == textures.end()) {
      textures[path] = LoadTexture(path.c_str());
    }
    return textures[path];
  }

  Texture2D load_player_texture_from_color(Color clr) {
    // create a unique key for the color
    std::string color_key = std::to_string(clr.r) + "_" + std::to_string(clr.g) + "_" + 
                           std::to_string(clr.b) + "_" + std::to_string(clr.a);

    // if we already loaded this color, return it
    if (colored_players.find(clr) != colored_players.end()) {
      return colored_players[clr];
    }

    // create a copy of the monochrome image to modify
    Image colored_image = ImageCopy(player_monochrome);

    Color enhanced_color;
    if (clr.r > clr.g && clr.g > clr.b) { // orange must be vibrant
        enhanced_color = {
            (unsigned char)std::min(255, (int)(clr.r * color_multiplier_r)),
            (unsigned char)std::min(255, (int)(clr.g * color_multiplier_g)),
            (unsigned char)std::min(255, (int)(clr.b * color_multiplier_b)),
            clr.a
        };
    } else {
        // boost primary colors more aggressively (still isn't working AAAH)
        enhanced_color = {
            (unsigned char)std::min(255, (int)(clr.r * color_multiplier_r)),
            (unsigned char)std::min(255, (int)(clr.g * color_multiplier_g)),
            (unsigned char)std::min(255, (int)(clr.b * color_multiplier_b)),
            clr.a
        };
    }

    // unmonochrome the image
    for (int y = 0; y < colored_image.height; y++) {
      for (int x = 0; x < colored_image.width; x++) {
        Color pixel = GetImageColor(colored_image, x, y);
        
        if (!((pixel.r == 255 && pixel.g == 255 && pixel.b == 255) || 
              (pixel.r == 0 && pixel.g == 0 && pixel.b == 0))) {
          float gray = (float)pixel.r / 255.0f;
          
          float enhanced_gray = powf(gray, 0.6f); // make mid-tones even brighter

          // enhance saturation
          Color new_color = {
            (unsigned char)(enhanced_color.r * enhanced_gray),
            (unsigned char)(enhanced_color.g * enhanced_gray),
            (unsigned char)(enhanced_color.b * enhanced_gray),
            pixel.a
          };
          
          ImageDrawPixel(&colored_image, x, y, new_color);
        }
      }
    }

    Texture2D colored_texture = LoadTextureFromImage(colored_image);
    UnloadImage(colored_image);

    // Cache the result
    colored_players[clr] = colored_texture;
    
    return colored_texture;
  }

  // on delete / out of scope
  ~ResourceManager() {
    UnloadImage(player_monochrome);
    for (auto &[path, tex] : textures) {
      UnloadTexture(tex);
    }
    for (auto &[color, tex] : colored_players) {
      UnloadTexture(tex);
    }
  }
};
